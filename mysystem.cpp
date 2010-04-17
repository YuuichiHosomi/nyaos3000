#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(NYADOS)
#  include <dos.h>
#elif defined(OS2EMX)
#  include <fcntl.h>
#  define INCL_DOSSESMGR
#  define INCL_DOSERRORS
#  define INCL_DOSFILEMGR
#  include <os2.h>
#else
#  include <fcntl.h>
#  include "windows.h"
#endif

#include "nndir.h"
#include "nnhash.h"
#include "nnstring.h"
#include "nnvector.h"
#include "shell.h"
#include "writer.h"

enum{
    TOO_LONG_COMMANDLINE = -3  ,
    MAX_COMMANDLINE_SIZE = 127 ,
};


/* ���spawn�Bspawn�̃C���^�[�t�F�C�X�� NN���C�u�����ɓK�����`�Œ񋟂���B
 *      args - �p�����[�^
 *      wait - P_WAIT , P_NOWAIT
 * return
 *      spawn �̖߂�l
 *      - wait == P_WAIT   �̎��́A�v���Z�X�̖߂�l
 *      - wait == P_NOWAIT �̎��́A�v���Z�XID
 */
static int mySpawn( const NnVector &args , int wait )
{
    /* �R�}���h������A�_�u���N�H�[�g������ */
    NnString cmdname(args.const_at(0)->repr());
    cmdname.dequote();

#ifdef OS2EMX
    const char **argv
        =(const char**)malloc( sizeof(const char *)*(args.size() + 1) );
    for(int i=0 ; i<args.size() ; i++)
        argv[i] = ((NnString*)args.const_at(i))->chars();
    argv[ args.size() ] = NULL;
    int result;

    if( wait == P_WAIT ){
        unsigned long type=0;
        int rc= DosQueryAppType( (unsigned char *)cmdname.chars() , &type );
        if( rc ==0  &&  (type & 7 )== FAPPTYP_WINDOWAPI){
            wait = P_PM;
        }
    }
    result = spawnvp(wait,(char*)NnDir::long2short(cmdname.chars()) , (char**)argv );
    free( argv );
#else
    extern int which( const char *nm, NnString &which );
    DWORD result;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    NnString fullpath_cmdname;

    memset(&si,0,sizeof(si));
    memset(&pi,0,sizeof(pi));

    if( NnDir::access(cmdname.chars())==0 ){
        fullpath_cmdname = cmdname;
    }else if( which( cmdname.chars() , fullpath_cmdname ) != 0 ){
        errno = ENOENT;
        return -1;
    }

    NnString cmdline;
    for(int i=0 ; i < args.size() ; ++i ){
        cmdline << args.const_at(i)->repr() << ' ';
    }
    cmdline.chop();
        
    if( ! CreateProcess(const_cast<CHAR*>(fullpath_cmdname.chars()) ,
                        const_cast<CHAR*>(cmdline.chars()) ,
                        NULL, 
                        NULL,
                        TRUE, /* �e�v���Z�X�̏����p�����邩�H */
                        0,    /* �쐬�t���O�H */
                        NULL, /* ���ϐ��̃|�C���^ */
                        NULL, /* �J�����g�f�B���N�g�� */
                        &si,
                        &pi) )
    {
        errno = ENOEXEC;
        return -1;
    }
    if( wait == P_NOWAIT ){
        result = pi.dwProcessId ;
    }else{
        WaitForSingleObject(pi.hProcess,INFINITE); 
        GetExitCodeProcess(pi.hProcess,&result);
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#endif
    return result;
}

/* ���_�C���N�g��������͂���B
 *    - >> �ł���΁A�A�y���h���[�h�ł��邱�Ƃ����o����
 * 
 *  cmdline - �R�}���h���C���ւ̃|�C���^
 *            �ŏ��́u>�v�́u��v�ɂ��邱�Ƃ�z��
 *  redirect - �ǂݎ�茋�ʂ��i�[����I�u�W�F�N�g
 *
 * return
 *    0  : ����
 *    -1 : ���s
 *  */
static int parseRedirect( const char *&cmdline , Redirect &redirect )
{
    const char *mode="w";
    if( *cmdline == '>' ){
	mode = "a";
	++cmdline;
    }
    NnString path;
    NyadosShell::readNextWord( cmdline , path );
    if( redirect.switchTo( path , mode ) != 0 ){
        return -1;
    }
    return 0;
}

/* ���_�C���N�g�� > �̉E���ڍs�̏������s��.
 *	sp : > �̎��̈ʒu
 *	redirect : ���_�C���N�g�I�u�W�F�N�g
 * return
 *      0  : ����
 *      -1 : ���s
 */
static int after_lessthan( const char *&sp , Redirect &redirect )
{
    if( sp[0] == '&'  &&  isdigit(sp[1]) ){
	/* n>&m */
        if( redirect.set( sp[1]-'0' ) != 0 )
            return -1;
	sp += 2;
    }else if( sp[0] == '&' && sp[1] == '-' ){
	/* n>&- */
        if( redirect.switchTo( "nul", "w" ) != 0 )
            return -1;
    }else{
	/* n> �t�@�C���� */
        if( parseRedirect( sp , redirect ) != 0 )
            return -1;
    }
    return 0;
}

/* �R�}���h���C���� | �ŕ������āA�x�N�^�[�Ɋe�R�}���h��������
 *	cmdline - �R�}���h���C��
 *	vector - �e�v�f������z��
 */
static void devidePipes( const char *cmdline , NnVector &vector )
{
    NnString one,left(cmdline);
    while( left.splitTo(one,left,"|") , !one.empty() )
	vector.append( one.clone() );
}

/* NYADOS ��p�F���_�C���N�g�����t���P�R�}���h�������[�`��
 *	cmdline - �R�}���h
 *	wait - P_WAIT or P_NOWAIT
 * return
 *	wait == P_WAIT �̎��F�v���Z�X�̎��s����
 *	wait == P_NOWAIT �̎��F�R�}���h�� ID
 *   -1 : ���_�C���N�g���s.
 */
static int do_one_command( const char *cmdline , int wait )
{
    Redirect redirect0(0);
    Redirect redirect1(1);
    Redirect redirect2(2);
    NnString execstr;

    int quote=0;
    while( *cmdline != '\0' ){
	if( *cmdline == '"' )
	    quote ^= 1;

        if( quote==0 && cmdline[0]=='<' ){
            ++cmdline;
            NnString path;
            NyadosShell::readNextWord( cmdline , path );
            if( redirect0.switchTo( path , "r" ) != 0 ){
                return -1;
            }
        }else if( quote==0 && cmdline[0]=='>' ){
	    ++cmdline;
            if( parseRedirect( cmdline , redirect1 ) != 0 )
                return -1;
	}else if( quote==0 && cmdline[0]=='1' && cmdline[1]=='>' ){
	    cmdline += 2;
	    if( after_lessthan( cmdline , redirect1 ) != 0 )
                return -1;
	}else if( quote==0 && cmdline[0]=='2' && cmdline[1]=='>' ){
	    cmdline += 2;
            if( after_lessthan( cmdline , redirect2 ) != 0 )
                return -1;
	}else{
	    execstr << *cmdline++;
	}
    }
    NnVector args;

    execstr.splitTo( args );
    return mySpawn( args , wait );
}

/* NYADOS ��p�Fsystem��֊֐��i�p�C�v���C�������j
 *	cmdline - �R�}���h������	
 *	wait - 1 �Ȃ�΃R�}���h�I���܂ő҂�
 *	       0 �Ȃ�ΑS�Ẵv���Z�X�͔񓯊��Ŏ��s����
 *	       (�����ăv���Z�XID���|�C���^�ɑ������)
 * return
 *	wait==1 : �Ō�Ɏ��s�����R�}���h�̃G���[�R�[�h
 *	wait==0 : �Ō�Ɏ��s�����R�}���h�̃v���Z�XID
 */
int mySystem( const char *cmdline , int wait=1 )
{
    if( cmdline[0] == '\0' )
	return 0;
    int pipefd0 = -1;
    int save0 = -1;
    NnVector pipeSet;
    int result=0;

    devidePipes( cmdline , pipeSet );
    for( int i=0 ; i < pipeSet.size() ; ++i ){
	errno = 0;
	/* NYACUS,NYAOS2 �ł́A�{���̃p�C�v���R�}���h��A������ */
        if( pipefd0 != -1 ){
	    /* �p�C�v���C�������ɍ���Ă���ꍇ�A
	     * ���͑���W�����͂֒���K�v������
             */

            /* �R�}���h���s���Z��������ꍇ�A��I�����Ă��܂��̂ŁA
             * �ȉ��R�[�h�͈ꎞ�폜 */
#if 0
            /* (���ɑO�̃R�}���h���G���[�I������Ȃ肵�Ă���ꍇ��
             *  �n���O�A�b�v���Ă��܂��̂ŁA�����R�}���h�𔭍s�����ɁA
             *  �����Œ��f����)
	     */
            if( _eof(save0) ){
                ::close( pipefd0 );
                pipefd0 = -1;
                goto exit;
            }
#endif
            if( save0 == -1  )
		save0 = dup( 0 );
            dup2( pipefd0 , 0 );
	    ::close( pipefd0 );
            pipefd0 = -1;
        }
        if( i < pipeSet.size() - 1 ){
	    /* �p�C�v���C���̖����łȂ��R�}���h�̏ꍇ�A
	     * �W���o�͂̐���A�p�C�v�̈���ɂ���K�v������
	     */
            int handles[2];

            int save1=dup(1);
#ifdef NYAOS2
            _pipe( handles );
#else
            _pipe( handles , 0 , O_BINARY | O_NOINHERIT );
#endif
            dup2( handles[1] , 1 );
	    ::close( handles[1] );
            pipefd0 = handles[0];

            result = do_one_command( ((NnString*)pipeSet.at(i))->chars(),P_NOWAIT);
            dup2( save1 , 1 );
            ::close( save1 );
        }else{
            result = do_one_command( ((NnString*)pipeSet.at(i))->chars(),
                    wait ? P_WAIT : P_NOWAIT );
            if( ! wait ){
                conErr << '<' << result << ">\n";
            }
        }
	if( result < 0 ){
	    if( ((NnString*)pipeSet.at(i))->length() > 110 ){
		conErr << "Too long command line,"
			    " or bad command or file name.\n";
	    }else if( errno != 0 ){
                conErr << strerror( errno ) << '\n';
	    }
            goto exit;
	}
    }
  exit:
    if( save0 >= 0 ){
        dup2( save0 , 0 );
    }
    return result;
}
