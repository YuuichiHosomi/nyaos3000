#include <assert.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#if defined(HAVE_NOT_OLD_NEW_H) || defined(_MSC_VER)
#  include <new>
#else
#  include <new.h>
#endif

#include "config.h"
#include "nnstring.h"
#include "getline.h"
#include "ishell.h"
#include "writer.h"

#define VER "20091219"

#ifdef _MSC_VER
#  include <windows.h>
#endif

#ifdef _MSC_VER
int  nya_new_handler(size_t size)
#else
void nya_new_handler()
#endif
{
    fputs("memory allocation error.\n",stderr);
    abort();
#ifdef _MSC_VER
    return 0;
#endif
}

/* ���ݏ������� rcfname �̖��O
 * �󂾂ƒʏ�� _nya ���Ăяo�����B
 */
static NnString rcfname;

/* rcfname_ �� _nya �Ƃ��ČĂяo�� */
static void callrc( const NnString &rcfname_ , const NnVector &argv )
{
    if( access( rcfname_.chars() , 0 ) != 0 ){
        /* printf( "%s: not found.\n",rcfname_.chars() ); */
        return;
    }

    rcfname = rcfname_;

    rcfname.slash2yen();
    ScriptShell scrShell( rcfname.chars() );

    scrShell.addArgv( rcfname );
    for( int i=0 ; i < argv.size() ; i++ ){
        if( argv.const_at(i) != NULL )
            scrShell.addArgv( *(const NnString *)argv.const_at(i) );
    }
    scrShell.mainloop();
}

/* rc�t�@�C����T���A���t���������̂���A�Ăяo���B
 *	argv0 - exe�t�@�C���̖��O.
 *      argv - �p�����[�^
 */
static void seek_and_call_rc( const char *argv0 , const NnVector &argv )
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

/* -d �f�o�b�O�I�v�V����
 *	argc , argv : main �̈���
 *	i : ���ݓǂݎ�蒆�� argv �̓Y��
 * return
 *	��� 0 (�p��)
 */
static int opt_d( int , char **argv , int &i )
{
    if( argv[i][2] == '\0' ){
	properties.put("debug",new NnString() );
    }else{
	size_t len=strcspn(argv[i]+2,"=");
	NnString var(argv[i]+2,len);
	if( argv[i][2+len] == '\0' ){
	    properties.put( var , new NnString() );
	}else{
	    properties.put( var , new NnString(argv[i]+2+len+1));
	}
    }
    return 0;
}
/* -r _nya �w��I�v�V��������
 *	argc , argv : main �̈���
 *	i : ���ݓǂݎ�蒆�� argv �̓Y��
 * return
 *	0 : �p�� 
 *	not 0 : NYA*S ���I��������(main�̖߂�l+1��Ԃ�)
 */
static int opt_r( int argc , char **argv, int &i )
{
    if( i+1 >= argc ){
	conErr << "-r : needs a file name.\n";
	return 2;
    }
    if( access( argv[i+1] , 0 ) != 0 ){
	conErr << argv[++i] << ": no such file.\n";
	return 2;
    }
    rcfname = argv[++i];
    return 0;
}
/* -e 1�s�R�}���h�p�I�v�V��������
 *	argc , argv : main �̈���
 *	i : ���ݓǂݎ�蒆�� argv �̓Y��
 * return
 *	0 : �p�� 
 *	not 0 : NYA*S ���I��������(main�̖߂�l+1��Ԃ�)
 */
static int opt_e( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << "-e needs commandline.\n";
	return 2;
    }
    OneLineShell sh( argv[++i] );
    sh.mainloop();
    return 1;
}

/* -f �X�N���v�g���s�p�I�v�V��������
 *	argc , argv : main �̈���
 *	i : ���ݓǂݎ�蒆�� argv �̓Y��
 * return
 *	0 : �p�� 
 *	not 0 : NYA*S ���I��������(main�̖߂�l+1��Ԃ�)
 */
static int opt_f( int argc , char **argv , int &i )
{
    if( i+1 >= argc ){
	conErr << argv[0] << " -f : no script filename.\n";
	return 2;
    }
    if( access(argv[i+1],0 ) != 0 ){
	conErr << argv[i+1] << ": no such file or command.\n";
	return 2;
    }
    NnString path(argv[i+1]);
    ScriptShell scrShell( path.chars() );

    for( int j=i+1 ; j < argc ; j++ )
	scrShell.addArgv( *new NnString(argv[j]) );
    int rc=1;
    if( argv[i][2] == 'x' ){
	NnString line;
	while(   (rc=scrShell.readline(line)) >= 0
	      && (strstr(line.chars(),"#!")  == NULL
	      || strstr(line.chars(),"nya") == NULL ) ){
	    // no operation.
	}
    }
    if( rc >= 0 )
	scrShell.mainloop();
    return 1;
}
/* -a �f�t�H���g�G�C���A�X��L���ɂ���
 *	argc , argv : main �̈���
 *	i : ���ݓǂݎ�蒆�� argv �̓Y��
 * return
 *	0 : �p�� 
 *	not 0 : NYA*S ���I��������(main�̖߂�l+1��Ԃ�)
 */
static int opt_a( int , char ** , int & )
{
    extern NnHash aliases;

    aliases.put("ls"    , new NnString("list") );
    aliases.put("mv"    , new NnString("move /-Y $@") );
    aliases.put("cp"    , new NnString("copy /-Y /B /V $@") );
    aliases.put("rmdir" , new NnString("rmdir $@") );
    aliases.put("rm"    , new NnString("del $@") );

    return 0;
}

int main( int argc, char **argv )
{
    // set_new_handler(nya_new_handler);
    properties.put("nyatype",new NnString(SHELL_NAME) );

    NnVector nnargv;
    for(int i=1;i<argc;i++){
	if( argv[i][0] == '-' ){
	    int rv;
	    switch( argv[i][1] ){
            case 'D': rv=opt_d(argc,argv,i); break;
	    case 'r': rv=opt_r(argc,argv,i); break;
            case 'f': rv=opt_f(argc,argv,i); break;
	    case 'e': rv=opt_e(argc,argv,i); break;
	    case 'a': rv=opt_a(argc,argv,i); break;
	    default:
		rv = 2;
		conErr << argv[0] << " : -" 
		    << (char)argv[1][1] << ": No such option\n";
		break;
	    }
	    if( rv != 0 )
		return rv-1;
	}else{
            nnargv.append( new NnString(argv[i]) );
	}
    }
    conOut << 
#ifdef ESCAPE_SEQUENCE_OK
"\x1B[2J" << 
#endif
	"Nihongo Yet Another OSes Shell 3000 v."VER" (C) 2001-09 by NYAOS.ORG\n";

    NnDir::set_default_special_folder();

    /* _nya �����s���� */
    if( rcfname.empty() ){
#ifdef _MSC_VER
        char execname[ FILENAME_MAX ];

        if( GetModuleFileName( NULL , execname , sizeof(execname)-1 ) > 0 ){
            seek_and_call_rc(execname,nnargv);
        }else{
#endif
            seek_and_call_rc(argv[0],nnargv);
#ifdef _MSC_VER
        }
#endif
    }else{
        callrc(rcfname,nnargv);
    }

    signal( SIGINT , SIG_IGN );

    /* DOS������̓��͂ɏ]���Ď��s���� */
    InteractiveShell intShell;

    /* �q�X�g�������[�h���� */
    NnString *histfn=(NnString*)properties.get("savehist");
    if( histfn != NULL ){
	FileReader fr( histfn->chars() );
	if( fr.eof() ){
	    perror( histfn->chars() );
	}else{
	    intShell.getHistoryObject()->read( fr );
	}
    }

    intShell.mainloop();

    /* �q�X�g����ۑ����� */
    if( (histfn=(NnString*)properties.get("savehist")) != NULL ){
        int histfrom=0;
        NnString *histsizestr=(NnString*)properties.get("histfilesize");
	History *hisObj=intShell.getHistoryObject();
        if( histsizestr != NULL ){
	    int histsize=atoi(histsizestr->chars());
	    if( histsize !=0 && (histfrom=hisObj->size() - histsize)<0 )
		histfrom = 0;
        }
        FileWriter fw( histfn->chars() , "w" );
        NnString his1;
        for(int i=histfrom ; i<hisObj->size() ; i++ ){
	    if( hisObj->get(i,his1) == 0 )
                (Writer&)fw << his1 << '\n';
        }
    }
    conOut << "\nGood bye!\n";
    return 0;
}

