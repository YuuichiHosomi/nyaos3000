#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include "shell.h"
#include "nnstring.h"
#include "nnhash.h"
#include "getline.h"
#include "reader.h"

/* �t�N�H�[�g�œǂ݂��߂镶����ʂ�߂� */
static int getQuoteMax()
{
    NnString *max=(NnString*)properties.get("backquote");
    return max == NULL ? 0 : 1024 ;
}

/* �R�}���h�̕W���o�͂̓��e�𕶎���Ɏ�荞��. 
 *   cmdline - ���s����R�}���h
 *   dst - �W���o�͂̓��e
 *   max - ���p�̏��. 0 �̎��͏������
 *   shrink - ��ȏ�̋󔒕�������ɂ��鎞�� true �Ƃ���
 * return
 *   0  : ����
 *   -1 : �I�[�o�[�t���[(max �܂ł� dst �Ɏ�荞��ł���)
 */
int eval_cmdline( const char *cmdline, NnString &dst, int max , bool shrink)
{
    /* �e���|�����t�@�C�����g���Ď��� */
    NnString tempfn=NnDir::tempfn();
    FILE *fp=fopen( tempfn.chars() , "w" );
    int tmpfd = dup(1);
    dup2( fileno(fp) , 1 );

    OneLineShell shell( cmdline );
    shell.mainloop();

    dup2( tmpfd , 1 );
    ::close( tmpfd );
    fclose(fp);

    FileReader pp( tempfn.chars() );
    
    int lastchar=' ';
    int ch;
    while( !pp.eof() && (ch=pp.getchr()) != EOF ){
        if( shrink && isSpace(ch) ){
            if( ! isSpace(lastchar) )
                dst += ' ';
        }else{
            dst += (char)ch;
        }
        lastchar = ch;
        if( max && dst.length() >= max )
            return -1;
    }
    dst.trim();
    ::remove( tempfn.chars() );
    return 0;
}

/* �t�N�H�[�g����
 *      sp  ��������(`�̎����w���Ă��邱��)
 *      dst �敶����
 *      max ���
 * return
 *      0  ����
 *      -1 �I�[�o�[�t���[
 */
static int doQuote( const char *&sp , NnString &dst , int max )
{
    NnString q;
    while( *sp != '\0' ){
        if( *sp =='`' ){
            /* �A������ `` �́A��� ` �֕ϊ�����B */
            if( *(sp+1) != '`' )
                break;
	    ++sp;
        }
	if( isKanji(*sp) )
	    q += *sp++;
        q += *sp++;
    }
    if( q.length() <= 0 ){
        dst += '`';
        return 0;
    }
    return eval_cmdline( q.chars() , dst , max , true );
}


/* ���_�C���N�g���ǂݎ��A���ʂ� FileWriter �I�u�W�F�N�g�ŕԂ�.
 * �I�[�v���ł��Ȃ��ꍇ�̓G���[���b�Z�[�W������ɏo��.
 *	sp �ŏ���'>'�̒���
 *         ���s��́A
 * return
 *      �o�͐�
 */
static FileWriter *readWriteRedirect( const char *&sp )
{

    // append ?
    const char *mode = "w";
    if( *sp == '>' ){
	++sp;
	mode = "a";
    }
    NnString fn;
    NyadosShell::readNextWord(sp,fn);
    if( fn.empty() ){
        conErr << "syntax error near unexpected token `newline'\n";
	return NULL;
    }

    FileWriter *fw=new FileWriter(fn.chars(),mode);

    if( fw != NULL && fw->ok() ){
	return fw;
    }else{
	perror( fn.chars() );
	delete fw;
	return NULL;
    }
}

/* sp ������t�H���_�[�����������񂩂�n�܂��Ă�����^��Ԃ�
 *	sp �c �擪�ʒu
 *	quote �c ��d���p�����Ȃ�^�ɂ���
 *	spc �c ���O�̕������󔒂Ȃ�^�ɂ��Ă���
 * return
 *	doFolder �ŕϊ����ׂ�������ł���΁A���̒���
 */
int isFolder( const char *sp , int quote , int spc )
{
    if( ! spc  ||  quote )
	return 0;

    if( *sp=='~' && 
        (getEnv("HOME")!=NULL || getEnv("USERPROFILE") != NULL ) && 
        properties.get("tilde")!=NULL)
    {
	int n=1;
	while( isalnum(sp[n] & 255) || sp[n]=='_' )
	    ++n;
	return n;
    }

    if( sp[0]=='.' && sp[1]=='.' && sp[2]=='.' && properties.get("dots")!=NULL ){
	int n=3;
	while( sp[n] == '.' )
	    ++n;
	return n;
    }
    return 0;
}
/* sp �Ŏn�܂����t�H���_�[����{���̃t�H���_�[���֕ϊ�����
 *	sp �c �擪�ʒu
 *	len �c ����
 *	dst �c �{���̃t�H���_�[��������Ƃ���
 *	quote �c ��d���p�����Ȃ�^�ɂ���
 */	
static void doFolder( const char *&sp , int len , NnString &dst , int & )
{
    NnString name(sp,len),value;
    NnDir::filter( name.chars() , value );
    sp += len;
    if( value.findOf(" \t\v\r\n") >= 0 ){
	dst << '"' << value << '"';
    }else{
	dst << value;
    }
}

/* ���ϐ��Ȃǂ�W�J����(�����R�}���h����)
 *	src - ��������
 *	dst - ���ʂ�����
 * return
 *      0 - ���� , -1 - ���s
 */
int NyadosShell::explode4internal( const NnString &src , NnString &dst )
{
    /* �p�C�v�܂��̏������Ɏ��s���� */
    NnString firstcmd; /* �ŏ��̃p�C�v�����܂ł̃R�}���h */
    NnString restcmd;  /* ����ڍs�̃R�}���h */
    src.splitTo( firstcmd , restcmd , "|" );
    if( ! restcmd.empty() ){
	if( restcmd.at(0) == '&' ){ /* �W���o��+�G���[�o�� */
	    NnString pipecmds( restcmd.chars()+1 );

            PipeWriter *pw=new PipeWriter(pipecmds);
            if( pw == NULL || ! pw->ok() ){
                perror( pipecmds.chars() );
                delete pw;
                return -1;
	    }
            conOut_ = pw ;
            conErr_ = new WriterClone(pw);
	}else{ /* �W���o�͂̂� */
            NnString pipecmds( restcmd.chars()+1 );

            PipeWriter *pw=new PipeWriter(pipecmds);
            if( pw == NULL || ! pw->ok() ){
                perror( pipecmds.chars() );
                delete pw;
                return -1;
	    }
            conOut_ = pw;
	}
    }
    int prevchar=' ';
    int quote=0;
    int len;
    dst.erase();
    int backquotemax=getQuoteMax();

    const char *sp=firstcmd.chars();
    while( *sp != '\0' ){

	if( *sp == '"' )
	    quote = !quote;
	
	int prevchar1=(*sp & 255);
	if( (len=isFolder(sp,quote,isSpace(prevchar))) != 0 ){
	    doFolder( sp , len , dst , quote );
        }else if( *sp == '>' && !quote ){
	    ++sp;
	    FileWriter *fw=readWriteRedirect( sp );
	    if( fw != NULL ){
                conOut_ = fw;
	    }else{
		conErr << "can't redirect stdout.\n";
		return -1;
	    }
	}else if( *sp == '1' && *(sp+1) == '>' && !quote ){
	    if( ! restcmd.empty() ){
		/* ���łɃ��_�C���N�g���Ă�����A�G���[ */
		conErr << "ambigous redirect.\n";
		return -1;
	    }
	    sp += 2;
	    if( *sp == '&'  &&  *(sp+1) == '2' ){
		sp += 2;
                conOut_ = new WriterClone(conErr_);
	    }else if( *sp == '&' && *(sp+1) == '-' ){
		sp += 2;
                conOut_ = new NullWriter();
	    }else{
		FileWriter *fw=readWriteRedirect( sp );
		if( fw != NULL ){
                    conOut_ = fw;
		}else{
		    conErr << "can't redirect stdout.\n";
		    return -1;
		}
	    }
	}else if( *sp == '2' && *(sp+1) == '>' && !quote ){
	    sp += 2;
	    if( *sp == '&' && *(sp+1) == '1' ){
		sp += 2;
                conErr_ = new WriterClone(conOut_);
	    }else if( *sp == '&' && *(sp+1) == '-' ){
		sp += 2;
                conErr_ = new NullWriter();
	    }else{
		FileWriter *fw=readWriteRedirect( sp );
		if( fw != NULL ){
                    conErr_ = fw;
		}else{
		    conErr << "can't redirect stderr.\n";
		    return -1;
		}
	    }
        }else if( *sp == '`' && backquotemax > 0 ){
	    ++sp;
            if( doQuote( sp , dst , backquotemax ) == -1 ){
                conErr << "over flow commandline.\n";
                return -1;
            }
	    ++sp;
	}else{
	    if( isKanji(*sp) )
		dst += *sp++;
	    dst += *sp++;
	}
	prevchar = prevchar1;
    }
    return 0;
}

/* �q�A�h�L�������g���s��
 *    sp - �u<<END�v�̍ŏ��� < �̈ʒu�������B
 *    dst - ����(�u< �ꎞ�t�@�C�����v��)������
 *    prefix - �u<�vor�u �v
 */
void NyadosShell::doHereDocument( const char *&sp , NnString &dst , char prefix )
{
    int quote_mode = 0 , quote = 0 ;
    sp += 2;

    /* �I�[�}�[�N��ǂݎ�� */
    NnString endWord;
    while( *sp != '\0' && (!isspace(*sp & 255) || quote ) ){
	if( *sp == '"' ){
	    quote ^= 1;
	    quote_mode = 1;
	}else{
	    endWord << *sp ;
	}
	++sp;
    }
    /* �h�L�������g�������ꎞ�t�@�C���ɏ����o�� */
    this->heredocfn=NnDir::tempfn();
    FILE *fp=fopen( heredocfn.chars() , "w" );
    if( fp != NULL ){
	VariableFilter  variable_filter(*this);
	NnString  line;

	NnString *prompt=new NnString(endWord);
	*prompt << '>';
	this->nesting.append( prompt );

	while( this->readline(line) >= 0 && !line.startsWith(endWord) ){
	    /* <<"EOF" �ł͂Ȃ��A<<EOF �`���̏ꍇ�́A
	     * %...% �`���̒P���u������K�v����
	     */
	    if( ! quote_mode )
		variable_filter( line );

	    fputs( line.chars() , fp );
	    if( ! line.endsWith("\n") )
		putc('\n',fp);
	    line.erase();
	}
	delete this->nesting.pop();
	fclose( fp );
	dst << prefix << heredocfn ;
    }
}


/* �O���R�}���h�p�v���v���Z�X.
 * �E%1,%2 ��ϊ�����
 * �E�v���O�������� / �� ���֕ϊ�����B
 *      src - ��������
 *      dst - ���H�������ʕ�����̓����
 * return 0 - ���� , -1 ���s(�I�[�o�[�t���[�Ȃ�)
 */
int NyadosShell::explode4external( const NnString &src , NnString &dst )
{
    /* �v���O���������t�B���^�[�ɂ�����F�`���_�ϊ��Ȃ� */
    NnString progname,args;
    src.splitTo( progname,args );
    NnDir::filter( progname.chars() , dst );
    if( args.empty() )
	return 0;

    dst << ' ';
    int backquotemax=getQuoteMax();
    int quote=0;
    int len;

    // �����̃R�s�[.
    int spc=1;
    for( const char *sp=args.chars() ; *sp != '\0' ; ++sp ){
        if( *sp == '"' )
            quote ^= 1;

	if( (len=isFolder(sp,quote,spc)) != 0 ){
	    doFolder( sp , len , dst , quote );
	    --sp;
        }else if( *sp == '`'  && backquotemax > 0 ){
	    ++sp;
            if( doQuote( sp , dst , backquotemax ) == -1 ){
                conErr << "Over flow commandline.\n";
                return -1;
            }
	}else if( *sp == '<' && *(sp+1) == '<'  &&  quote == 0 ){
	    /* �q�A�h�L�������g */
	    doHereDocument( sp , dst , '<' );
	}else if( *sp == '<' && *(sp+1) == '=' && quote == 0 ){
	    /* �C�����C���t�@�C���e�L�X�g */
	    doHereDocument( sp , dst , ' ' );
	}else if( *sp == '|' && *(sp+1) == '&' ){
	    dst << "2>&1 |";
        }else{
	    if( isKanji(*sp) )
		dst += *sp++;
            dst += *sp;
        }
        spc = isSpace( *sp );
    }
    return 0;
}
