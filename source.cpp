#include <unistd.h>
#include "nua.h"
#include "source.h"
#include "reader.h"
#include "writer.h"
#include "shell.h"

struct ReaderAndBuffer {
    Reader *reader;
    NnString line;
};

/* Lua ����\�[�X��ǂނ��߂̃R�[���o�b�N�֐� */
static const char *reader_for_lua(lua_State *L , void *data , size_t *size )
{
    ReaderAndBuffer *rb = (ReaderAndBuffer*)data;
    if( rb->reader->eof() || rb->reader->readLine(rb->line) < 0 )
        return NULL;
    rb->line << '\n';
    *size = rb->line.length();
    return rb->line.chars();
}

/* �ʏ�\�[�X�ELua�\�[�X���ʓǂݍ��ݏ��� */
static int do_source( const NnString &cmdname , const NnVector &argv )
{
    /* �usource �c�v�F�R�}���h�ǂ݂��� */
    FileReader *fr=new FileReader(cmdname.chars());
    if( fr == 0 || fr->eof() ){
        conErr << cmdname << ": no such file or directory.\n";
        delete fr;
        return -1;
    }

    fpos_t pos;
    NnString line;

    fr->getpos(pos);
    fr->readLine(line);
    fr->setpos(pos);

    if( line.startsWith("--") ){
        /* Lua-code */
        ReaderAndBuffer rb;
        rb.reader = fr;

        lua_State *L=nua_init();
        if( lua_load(L , reader_for_lua , &rb, cmdname.chars()) != 0 ||
            lua_pcall( L , 0 , 0 , 0 ) != 0 )
        {
            conErr << cmdname.chars() << ": " << lua_tostring(L,-1) << '\n';
            lua_pop(L,1);
        }
        delete fr;
    }else{
        ScriptShell scrShell( fr ); /* fr �̍폜�`���� scrShell �ֈϏ� */
        if( scrShell ){
            scrShell.addArgv( cmdname );
            for( int i=0 ; i < argv.size() ; i++ ){
                if( argv.const_at(i) != NULL )
                    scrShell.addArgv( *(const NnString *)argv.const_at(i) );
            }
            scrShell.mainloop();
        }
    }
    return 0;
}

/* �V�F���t�@�C���̓ǂݍ���.
 * return
 *	��� 0
 */
int cmd_source( NyadosShell &shell , const NnString &argv )
{
    NnString arg1,left;
    argv.splitTo( arg1 , left );
    NyadosShell::dequote( arg1 );

    if( arg1.compare("-h") == 0 ){
	/* �usource -h �c�v�F�q�X�g���ǂ݂��� */
	NyadosShell::dequote( left );
	FileReader fr( left.chars() );
	if( fr.eof() ){
	    conErr << left << ": no such file or directory.\n";
	}else{
	    shell.getHistoryObject()->read( fr );
	}
    }else{
        NnVector argv;

        NnString t(arg1);
        while( ! t.empty() ){
            argv.append( t.clone() );
            left.splitTo( t , left );
        }
        return do_source( arg1 , argv );
    }
    return 0;
}

/* ���ݏ������� rcfname �̖��O
 * �󂾂ƒʏ�� _nya ���Ăяo�����B
 */
NnString rcfname;

/* nyaos.rcfname �Ƃ��� Lua �ϐ��Ƀt�@�C�������Z�b�g���� */
static void set_rcfname_for_Lua( const char *fname )
{
    lua_State *lua = nua_init();

    lua_getglobal(lua,"nyaos");
    if( lua_istable(lua,-1) ){
        if( fname != NULL ){
            lua_pushstring(lua,fname);
        }else{
            lua_pushnil(lua);
        }
        lua_setfield(lua,-2,"rcfname");
    }
    lua_pop(lua,1); /* drop nyaos */
}


/* rcfname_ �� _nya �Ƃ��ČĂяo�� */
void callrc( const NnString &rcfname_ , const NnVector &argv )
{
    if( access( rcfname_.chars() , 0 ) != 0 ){
        /* printf( "%s: not found.\n",rcfname_.chars() ); */
        return;
    }

    rcfname = rcfname_;
    rcfname.slash2yen();
    set_rcfname_for_Lua( rcfname.chars() );

    do_source( rcfname , argv );

    set_rcfname_for_Lua( NULL );
}

/* rc�t�@�C����T���A���t���������̂���A�Ăяo���B
 *	argv0 - exe�t�@�C���̖��O.
 *      argv - �p�����[�^
 */
void seek_and_call_rc( const char *argv0 , const NnVector &argv )
{
    /* EXE �t�@�C���Ɠ����f�B���N�g���� _nya �����݂� */
    NnString rcfn1;

    int lastroot=NnDir::lastRoot( argv0 );
    if( lastroot != -1 ){
        rcfn1.assign( argv0 , lastroot );
        rcfn1 << '\\' << RUN_COMMANDS;
        callrc( rcfn1 , argv );
    /* / or \ �����t����Ȃ���������
     * �J�����g�f�B���N�g���Ƃ݂Ȃ��Ď��s���Ȃ�
     * (��d�Ăяo���ɂȂ�̂�)
     */
#if 0
    }else{
        printf("DEBUG: '%s' , (%d)\n", argv0 , lastroot);
#endif
    }
    /* %HOME%\_nya or %USERPROFILE%\_nya or \_nya  */
    NnString rcfn2;

    const char *home=getEnv("HOME");
    if( home == NULL )
        home = getEnv("USERPROFILE");

    if( home != NULL ){
        rcfn2 = home;
        rcfn2.trim();
    }
    rcfn2 << "\\" << RUN_COMMANDS;
    if( rcfn2.compare(rcfn1) != 0 )
        callrc( rcfn2 , argv );

    /* �J�����g�f�B���N�g���� _nya �����݂� */
    NnString rcfn3;
    char cwd[ FILENAME_MAX ];

    getcwd( cwd , sizeof(cwd) );
    rcfn3 << cwd << '\\' << RUN_COMMANDS ;

    if( rcfn3.compare(rcfn1) !=0 && rcfn3.compare(rcfn2) !=0 )
        callrc( rcfn3 , argv );
}
