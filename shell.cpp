#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#if defined(NYADOS)
#  include <dos.h>
#endif
#include <string.h>
#ifndef OS2EMX
#  include <dir.h>
#endif
#include <errno.h>

#include "nnvector.h"
#include "nnstring.h"
#include "nnhash.h"
#include "shell.h"
#include "getline.h"
#include "nua.h"

// #define TRACE 1

extern NnHash properties;

/* �G�C���A�X��ێ�����n�b�V���e�[�u�� */
NnHash aliases;
NnHash functions;

class NyadosCommand : public NnObject {
    int (*proc)( NyadosShell &shell , const NnString &argv );
public:
    NyadosCommand( int (*p)(NyadosShell &,const NnString &) ) : proc(p) {}
    int run(NyadosShell &s,const NnString &argv){ return (*proc)(s,argv);  }
};

int preprocessHistory( History &hisObj , const NnString &src , NnString &dst );
int mySystem( const char *cmdline , int wait=1 );
int which( const char *nm, NnString &which );
void brace_expand( NnString &s );

void NnStrFilter::operator() ( NnString &target )
{
    NnString result;

    NnString::Iter itr(target);
    this->filter( itr , result );
    target = result;
}

static void semi2spc( NnString::Iter &it , NnString &value )
{
    NnString temp;
    int has_spc=0;
    for(; *it ; ++it ){
	if( *it == ';' ){
	    if( has_spc ){
		value << '"' << temp << '"' ;
	    }else{
		value << temp ;
	    }
	    value << ' ';
	    temp.erase() ; has_spc = 0;
	}else{
	    if( it.space() )
		has_spc = 1;
	    temp << *it;
	}
    }
    if( has_spc ){
	value << '"' << temp << '"';
    }else{
	value << temp ;
    }
}

int VariableFilter::lookup( NnString &name , NnString &value )
{
    /* �ϐ������� .length �� .suffix �Ȃǂ̊g���������擾���� */
    NnString base,suffix;
    for( NnString::Iter it(name) ; *it ; ++it ){
	if( *it == '.' ){
	    if( base.length() > 0 )
		base << '.';
	    base << suffix;
	    suffix.erase();
	}else{
	    suffix << *it;
	}
    }
    if( base.length() <= 0 ){
	base = suffix;
	suffix.erase();
    }

    NnString lowname(base) , upname(base) ;
    lowname.downcase(); upname.upcase();

    
    NnString *s = (NnString*)properties.get(lowname) ;
    if( s != NULL ){
	/* �I�v�V������`����Ă����ꍇ */
	if( suffix.compare("defined") == 0 ){
	    value = "1";
	}else if( suffix.compare("length") == 0 ){
	    value.addValueOf( s->length() );
	}else if( suffix.compare("split") == 0 ){
	    NnString::Iter temp(*s);
	    semi2spc( temp , value );
	}else{
	    value = *s;
	}
	return 0;
    }
    const char *p=getEnv(upname.chars(),NULL);
    if( p != NULL ){
	/* ���ϐ���`����Ă��鎞 */
	if( suffix.compare("defined") == 0 ){
	    value = "2";
	}else if( suffix.compare("length") == 0 ){
	    value.addValueOf( strlen(p) );
	}else if( suffix.compare("split") == 0 ){
	    NnString::Iter temp(p);
	    semi2spc( temp , value );
	}else{
	    value = p;
	}
	return 0;
    }
    /* ������`����Ă��Ȃ��� */
    if( suffix.compare("defined") == 0 ){
	value = "0";
    }else if( suffix.compare("length") == 0 ){
	value = "0";
    }else if( suffix.compare("split") == 0 ){
	value ="";
    }else{
	return 1;
    }
    return 0;
}

/* %0 %1 %2 �Ȃǂ̈����W�J�p */
void VariableFilter::cnv_digit( NnString::Iter &p , NnString &result )
{
    const NnString *value;
    int n=0;

    do{
	n = n*10 + (*p-'0');
	++p;
    }while( p.digit() );

    if( n < shell.argc()  &&  (value=shell.argv(n)) != NULL )
	result << *value;
}

/* %* $* �Ȃǂ̓W�J�p */
void VariableFilter::cnv_asterisk( NnString::Iter &p , NnString &result )
{
    for(int j=1;j< shell.argc();j++){
	const NnString *value=shell.argv(j);
	if( value != NULL )
	    result << *value << ' ';
    }
    ++p;
}

void VariableFilter::filter( NnString::Iter &p , NnString &result )
{
    int quote=0;
    while( *p ){
	if( *p == '"' ){
	    quote ^= 1;
	    result << *p;
	    ++p;
	}else if( *p == '%' ){
	    NnString name , value;
	    ++p;
	    if( p.digit() ){
		cnv_digit( p , result );
	    }else if( *p == '*' ){
		cnv_asterisk( p , result );
	    }else{
		for(; *p != 0 && *p != '%' ; ++p )
		    name << *p;
		if( lookup(name,value)==0 ){
		    result << value;
		}else{
		    result << '%' << name << *p;
		}
		++p;
	    }
	}else if( *p == '$' && !quote ){
	    NnString name , value;
	    ++p;
	    if( p.digit() ){
		cnv_digit( p , result );
	    }else if( *p == '$' ){
		result << '$';
		++p;
	    }else if( *p == '*' ){
		cnv_asterisk( p , result );
	    }else if( *p == '{' ){
		while( ++p , *p && *p != '}' ){
		    name << *p ;
		}
		if( lookup(name,value)==0 ){
		    result << value;
		}else{
		    result << "${" << name << *p;
		}
		++p;
	    }else{
		for( ; p.alnum() ; ++p )
		    name << *p;
		if( lookup(name,value)==0 ){
		    result << value;
		}else{
		    result << '$' << name ;
		}
	    }
	}else{
	    result << *p;
	    ++p;
	}
    }
}


/* �R�}���h������֘A�t���R�}���h���擾����B
 *      p - �R�}���h��
 * return
 *      ���s�R�}���h(�����ꍇ�� NULL )
 */
static NnString *getInterpretor( const char *p )
{
    int lastdot=NnString::findLastOf(p,"/\\.");
    if( lastdot == -1  ||  p[lastdot] != '.' )
	return NULL;
    
    NnString suffix;
    NyadosShell::dequote( p+lastdot+1 , suffix );
    return (NnString*)DosShell::executableSuffix.get(suffix);
}

/* �󔒂̌�́u-�v�� �u/�v��
 * �u/�v���u\\�v�ɒu��������.
 *	src (i) : ��������
 *	dst (i) : �ϊ���
 */
static void cnv_sla_opt( const NnString &src , NnString &dst )
{
    int spc=1;
    for( NnString::Iter p(src) ; *p ; ++p ){
	if( spc && *p == '-' ){
	    dst << '/';
	}else if( *p == '/' ){
	    dst << '\\';
	    if( p.space() || *p =='\0' )
		dst << '.';
	}else if( *p == '\\' && (isspace(*p.next) || *p.next=='\0' ) ){
	    dst << "\\.";
	}else{
	    dst << *p;
	}
	spc = p.space();
    }
}

/* $n ��W�J����B
 *      p       - '$' �̈ʒu
 *      param   - �������X�g($1�Ȃǂ̌��l�^)
 *      argv    - ������S�ĘA����������
 *      replace - ����
 */
static void dollar( const char *&p , NnVector &param ,
                    NnString &argv , NnString &replace )
{
    switch( p[1] ){
    default:           replace << '$';  ++p;    break;
    case '*':          replace << argv; p += 2; break;
    case '$':          replace << '$';  p += 2; break;
    case 't':case 'T': replace << " ;"; p += 2; break;
    case 'b':case 'B': replace << '|';  p += 2; break;
    case 'g':case 'G': replace << '>';  p += 2; break;
    case 'l':case 'L': replace << '<';  p += 2; break;
    case 'q':case 'Q': replace << '`';  p += 2; break;
    case 'p':case 'P': replace << '%';  p += 2; break;
    case '@': cnv_sla_opt(argv,replace);p += 2; break;

    case '0':case '1':case '2':case '3':case '4':
    case '5':case '6':case '7':case '8':case '9':
	/* $1,$2 �Ȃ� */
	int n=(int)strtol(p+1,(char**)&p,10); 
	NnString *s=(NnString*)param.at(n);
	if( s != NULL )
	    replace << *s;
	if( *p == '*' ){
	    /* �u$1*�v�Ȃ� */
	    while( ++n < param.size() )
		replace << ' ' << *(NnString*)param.at(n);
	    ++p ;
	}
	break;
    }
}

int OneLineShell::readline( NnString &buffer )
{
    if( cmdline.empty() )
	return -1;
    buffer = cmdline;
    cmdline.erase();
    return buffer.length();
}

int OneLineShell::operator !() const
{
    if( cmdline.empty() )
	return !*parent_;
    return 0;
}

static void replaceDollars( const NnString *aliasValue ,
			    NnVector &param ,
			    NnString &argv ,
			    NnString &replace )
{
    int dollarflag=0;
    for( const char *p=aliasValue->chars() ; *p != '\0' ; ){
	if( *p == '$' ){
	    dollar( p , param , argv , replace );
	    dollarflag = 1;
	}else{
	    replace << *p++ ;
	}
    }
    if( ! dollarflag )
       replace << ' ' << argv;
}

int sub_brace_start( NyadosShell &bshell , 
		     const NnString &arg1 ,
		     const NnString &argv );

/* �u ;�v���ŋ�؂�ꂽ��̂P�R�}���h�����s����B
 * (interpret1 ����Ăяo����܂�)
 *	replace - �R�}���h
 * return
 *	�I���R�[�h
 */
int NyadosShell::interpret2( const NnString &replace_ )
{
    NnString replace(replace_);
    VariableFilter variable_filter( *this );

    /* ���ϐ��W�J */
    variable_filter( replace );

    NnString arg0 , argv , arg0low ;

    replace.splitTo( arg0 , argv );

    /* �֐���`�� */
    if( arg0.endsWith("{") ){
	sub_brace_start( *this , arg0 , argv );
	return 0;
    }
    arg0low = arg0;
    arg0low.downcase();

    NyadosCommand *cmdp = (NyadosCommand*)command.get( arg0low );
    if( cmdp != NULL ){
	/* �����R�}���h�����s���� */
        NnString argv2;
        if( explode4internal( argv , argv2 ) != 0 )
            return 0;

        int rc=cmdp->run( *this , argv2  );

        // �W���o��/�G���[�o��/���͂����ɖ߂�.
        this->setOut( new AnsiConsoleWriter(stdout) );
	this->setErr( new AnsiConsoleWriter(stderr) );
	this->setIn(  new StreamReader(stdin ) );
        if( rc != 0 )
            return -1;
    }else{
#ifdef LUA_ENABLE
        lua_State *lua = nua_init();

        if( (lua_getglobal(lua,"nyaos"),    lua_type(lua,-1)) == LUA_TTABLE  &&
            (lua_getfield(lua,-1,"command"),lua_type(lua,-1)) == LUA_TTABLE )
        {
            // fputs("Enter: Found Table\n",stderr);
            lua_getfield(lua,-1,arg0low.chars());
            if( lua_type(lua,-1) == LUA_TFUNCTION ){
                // fputs("Enter: Found Function\n",stderr);
                lua_pushstring(lua,argv.chars());
                if( lua_pcall(lua,1,0,0) != 0 ){
                    const char *msg = lua_tostring( lua , -1 );
                    this->err() << msg << '\n';
                }
                lua_settop(lua,0);
                return 0;
            }
        }
        lua_settop(lua,0);
#endif
        NnExecutable *func = (NnExecutable*)functions.get(arg0low);
	// BufferedShell *bShell = (BufferedShell*)
	if( func != NULL ){
	    /* �T�u�V�F�������s���� */
	    NnVector param;
	    param.append( arg0.clone() );
	    argv.splitTo( param );
            (*func)( param );
	}else{
	    /* �O���R�}���h�����s���� */
	    NnString cmdline2;
	    if( explode4external( replace , cmdline2 ) != 0 )
		goto exit;

	    exitStatus_ = mySystem( cmdline2.chars() );
	}
    }
  exit:
    if( ! heredocfn.empty() ){
	::remove( heredocfn.chars() );
	heredocfn.erase();
    }
    return 0;
}

/* �����񒆂� " �̐��𐔂��� */
static int countQuote( const char *p )
{
    int qc=0;
    for( ; *p != '\0' ; ++p ){
	if( *p == '"' )
	    ++qc;
    }
    return qc;
}

/* �\�[�X����A�P�R�}���h���؂�o���B
 * �؂�o�������e�͊�{�I�Ƀq�X�g���u������邾���ŁA���̉��H�͂Ȃ��B
 *    buffer - ���o�����R�}���h������o�b�t�@�B
 */
int NyadosShell::readcommand( NnString &buffer )
{
#ifdef TRACE
    fprintf(stderr,"NyadosShell::readcommand(\"%s\")\n",buffer.chars() );
#endif
    if( current.empty() ){
	NnString temp;
	int rc;

	/* �ŏ��̂P�s�擾 */
	for(;;){
	    rc=readline( current );
	    if( rc < 0 )
		return rc;
	    current.trim();
	    if( current.length() > 0 && ! current.startsWith("#") )
		break;
	    /* �R�����g�s�̏ꍇ�́A������x�擾���Ȃ��� */
	    current.erase();
	}
	for(;;){
	    if( rc < 0 ){
		return rc;
	    }
	    if( properties.get("history") != NULL ){
		History *hisObj =this->getHistoryObject();
		int isReplaceHistory=1;

		if( hisObj == NULL  &&  parent_ != NULL ){
		    hisObj = parent_->getHistoryObject();
		    isReplaceHistory = 0;
		}

		/* �q�X�g�������݂���΁A�q�X�g���u�����s�� */
		if( hisObj != NULL ){
		    NnString result;
		    if( preprocessHistory( *hisObj , current , result ) != 0 ){
			fputs( result.chars() , stderr );
			if( isReplaceHistory )
			    hisObj->drop(); // ���͎��̂��폜����.
			return 0;
		    }
		    if( isReplaceHistory ){
			hisObj->set(-1,result);
			hisObj->pack();
		    }
		    current = result;
		}
		
	    }
	    if( properties.get("bracexp") != NULL )
		brace_expand( current );
		
	    current.trim();
	    if( countQuote(current.chars()) % 2 != 0 ){
		nesting.append(new NnString("quote>"));
	    }else if( current.endsWith("^") ){
                // lastchar() == '^'
		nesting.append(new NnString("more>"));
		current.chop();
	    }else{
		break;
	    }
	    /* �p���s������̂ł���΁A�����Ď擾
	     * (�p���s�ɂ́A�R�����g�͂��肦�Ȃ����̂Ƃ���)
	     */
	    temp.erase();
	    rc = readline( temp );
	    delete nesting.pop();
	    temp.trim();
	    current << '\n' << temp;
	}
    }
    int quote=0;
    int i=0;
    int prevchar=0;
    for(;;){
	if( i>=current.length() ){
	    current.erase();
	    break;
	}
        if( !quote ){
            /* �R�}���h�^�[�~�l�[�^�u ;�v��������! */
            if( !quote  && isSpace(current.at(i)) && current.at(i+1)==';' ){
                current = current.chars()+i+2;
                break;
            }
            /* �R�}���h�^�[�~�l�[�^�u&�v��������! */
            if( !quote  && prevchar != '>' && prevchar != '|' && current.at(i)=='&' ){
                if( i+1 < current.length() && current.at(i+1) == '&' ){
                    buffer << "&&";
                    i+=2;
                    continue;
                }
                if( i+1>=current.length() || current.at(i+1) != '&'  ){
                    buffer << '&'; // �t���O�̈Ӗ��ł킴�Ǝc���Ă���...
                    current = current.chars()+i+1;
                    break;
                }
            }
        }
	if( current.at(i) == '"' ){
	    quote ^= 1;
	}
	if( isKanji(current.at(i)) ){
	    buffer << (char)(prevchar = current.at(i++));
	    buffer << (char)current.at(i++);
	}else{
	    buffer << (char)(prevchar = current.at(i++));
	}
    }
    return 0;
}

/* which �̃��b�p�[�F�p�X�ɋ󔒕������܂܂�Ă���ꍇ�A���p���ň͂ށB
 *	fn : �t�@�C����
 *	fullpath : �t���p�X��
 * return
 *	0: ���� , !0 : ���s
 */
static int which_pathquote( const char *fn , NnString &fullpath )
{
    NnString fn_;
    NyadosShell::dequote( fn , fn_ );
    int rc=which(fn_.chars(),fullpath);
    if( rc==0  &&  fullpath.findOf(" \t") >= 0 ){
	fullpath.insertAt(0,"\"");
	fullpath << '"';
    }
    return rc;
}

/* �C���^�[�v���^���}��
 *	 start - "" or "start " ��
 *	 script - �X�N���v�g��
 *	 argv - ����
 *       buffer - �}����o�b�t�@
 * return
 *       0 - ���� , !0 - ���s(replace �ύX�Ȃ�)
 */
static int insertInterpreter( const char *start ,
			      const char *script ,
			      const NnString &argv ,
			      NnString &buffer )
{
    NnString *intnm=getInterpretor( script );
    if( intnm == NULL )
	return 1; /* �g���q�Y���Ȃ� */

    buffer << start << *intnm << ' ';

    /* �X�N���v�g�̃t���p�X���擾���� */
    NnString fullpath;
    if( which_pathquote(script,fullpath) != 0 ){
	buffer << script;
    }else{
	buffer << fullpath;
    }
    buffer << ' ' << argv ;
    return 0;
}

/* ��s�C���^�[�v���^.
 * �E����������(�R�}���h�s)�� ; �ŕ������A���̂��̂� interpret2 �ŏ�������.
 *      statement : �R�}���h�s
 * return
 *      0 �p�� -1 �I��
 */
int NyadosShell::interpret1( const NnString &statement )
{
#ifdef TRACE
    fprintf(stderr,"NyadosShell::interpret1(\"%s\")\n",statement.chars());
#endif
    NnString cmdline(statement);
#ifdef LUA_ENABLE
    lua_State *lua=nua_init();

    if( (lua_getglobal(lua,"nyaos")   ,lua_type(lua,-1)) == LUA_TFUNCTION &&
        (lua_getfield(lua,-1,"filter"),lua_type(lua,-1)) == LUA_TFUNCTION )
    {
        lua_pushstring(lua,statement.chars());
        if( lua_pcall(lua,1,1,0) == 0 ){
            switch( lua_type(lua,-1) ){
            case LUA_TSTRING:
                cmdline = lua_tostring(lua,-1); 
                break;
            }
        }else{
            const char *msg = lua_tostring( lua , -1 );
            this->err() << msg << '\n';
        }
    }
    lua_settop(lua,0);
#endif

    errno = 0;
    cmdline.trim();
    if( cmdline.length() <= 0 )
        return 0;
    /* �R�����g������ readcommand �֐����x���ŏ����ς� */

    if( cmdline.length()==2 && cmdline.at(1)==':' && isalpha(cmdline.at(0) & 0xFF) ){
	NnDir::chdrive( cmdline.at(0) );
        return 0;
    }else if( cmdline.at(0) == ':' ){
        NnString label( cmdline.chars()+1 );

        label.trim();
        label.downcase();
        if( label.length() > 0 )
            setLabel( label );
        return 0;
    }

    NnString arg0,arg0low;
    NnString argv;
    NnString replace;
    if( cmdline.endsWith("&") ){
	NnString *startStr=(NnString*)properties.get("start");
	if( startStr != NULL ){
	    replace << *startStr << ' ';
	}else{
	    replace << "start ";
	}
	cmdline.chop();
    }

    NnString rest(cmdline);
    while( ! rest.empty() ){
	NnString cmds;
	int dem=rest.splitTo(cmds,rest,"|");
	if( rest.at(0) == '&' ){
	    /* |& �ɑ΂���Ή� */
	    rest.shift();
	    cmds << " 2>&1";
	}

	cmds.splitTo( arg0 , argv );
	arg0.slash2yen();
        arg0low = arg0;
        arg0low.downcase();
        
	NnString *aliasValue=(NnString*)aliases.get(arg0low);
        if( aliasValue != NULL ){// �G�C���A�X�������B
            /* �p�����[�^�𕪉����� */
            NnVector param;
            param.append( arg0.clone() );
	    argv.splitTo( param );
	    replaceDollars( aliasValue , param , argv , replace );
	}else{
	    if( insertInterpreter("",arg0low.chars(),argv,replace) != 0 )
		replace << arg0 << ' ' << argv ;
	}
	
	if( dem == '|' )
	    replace << " | ";
    }
    return this->interpret2( replace );
}

int NyadosShell::interpret( const NnString &statement )
{
    const char *p=statement.chars();
    NnString replace;
    int quote=0;

    while( *p != '\0' ){
        if( quote == 0 ){
            if( p[0] == '&' && p[1] == '&' ){
                int result = this->interpret1( replace );
                if( this->exitStatus() != 0 ){
                    return result;
                }
                replace.erase();
                p += 2;
                continue;
            }
            if( p[0] == '|' && p[1] == '|' ){
                int result = this->interpret1( replace );
                if( this->exitStatus() == 0 ){
                    return result;
                }
                replace.erase();
                p += 2;
                continue;
            }
        }
        if( *p == '"' ){
            quote ^= 1;
        }
        replace << *p++;
    }
    return this->interpret1( replace );
}


/* �V�F���̃��C�����[�v�B
 *	�V�F���R�}���h�̎擾��� readline�֐��Ƃ��ĉ��z�����Ă���B
 * return
 *	0:���� , -1:�l�X�g���߂�
 */
int NyadosShell::mainloop()
{
    static int nesting=0;

    if( nesting >= MAX_NESTING ){
	fputs("!!! too deep nesting shell !!!\n",stderr);
	return -1;
    }

    ++nesting;
	NnString cmdline;
	do{
	    cmdline.erase();
	}while( readcommand(cmdline) >= 0 && interpret(cmdline) != -1 );
    --nesting;
    return 0;
}

int cmd_ls      ( NyadosShell & , const NnString & );
int cmd_bindkey ( NyadosShell & , const NnString & );
int cmd_endif   ( NyadosShell & , const NnString & );
int cmd_else    ( NyadosShell & , const NnString & );
int cmd_if      ( NyadosShell & , const NnString & );
int cmd_set     ( NyadosShell & , const NnString & );
int cmd_exit    ( NyadosShell & , const NnString & );
int cmd_source  ( NyadosShell & , const NnString & );
int cmd_echoOut ( NyadosShell & , const NnString & );
int cmd_echoErr ( NyadosShell & , const NnString & );
int cmd_chdir   ( NyadosShell & , const NnString & );
int cmd_goto    ( NyadosShell & , const NnString & );
int cmd_shift   ( NyadosShell & , const NnString & );
int cmd_alias   ( NyadosShell & , const NnString & );
int cmd_unalias ( NyadosShell & , const NnString & );
int cmd_suffix  ( NyadosShell & , const NnString & );
int cmd_sub     ( NyadosShell & , const NnString & );
int cmd_unsuffix( NyadosShell & , const NnString & );
int cmd_open    ( NyadosShell & , const NnString & );
int cmd_option  ( NyadosShell & , const NnString & );
int cmd_unoption( NyadosShell & , const NnString & );
int cmd_history ( NyadosShell & , const NnString & );
int cmd_pushd   ( NyadosShell & , const NnString & );
int cmd_popd    ( NyadosShell & , const NnString & );
int cmd_dirs    ( NyadosShell & , const NnString & );
int cmd_foreach ( NyadosShell & , const NnString & );
int cmd_pwd     ( NyadosShell & , const NnString & );
int cmd_folder  ( NyadosShell & , const NnString & );
int cmd_xptest  ( NyadosShell & , const NnString & );
int cmd_lua_e   ( NyadosShell & , const NnString & );

int cmd_eval( NyadosShell &shell , const NnString &argv )
{
    NnString tmp(argv);
    OneLineShell oShell(&shell,tmp);
    oShell.mainloop();
    return 0;
}

/* �V�F���̃I�u�W�F�N�g�̃R���X�g���N�^ */
NyadosShell::NyadosShell( NyadosShell *parent )
{
    const static struct Command {
	const char *name;
	int (*proc)( NyadosShell & , const NnString & );
    } cmdlist[]={
	{ "alias"   , &cmd_alias    },
	{ "bindkey" , &cmd_bindkey  },
	{ "cd"	    , &cmd_chdir    },
	{ "dirs"    , &cmd_dirs     },
	{ "else"    , &cmd_else     },
	{ "endif"   , &cmd_endif    },
	{ "eval"    , &cmd_eval     },
	{ "exit"    , &cmd_exit     },
	{ "folder"  , &cmd_folder   },
	{ "foreach" , &cmd_foreach  },
	{ "goto"    , &cmd_goto     },
	{ "history" , &cmd_history  },
	{ "if"      , &cmd_if       },
	{ "list"    , &cmd_ls       },
	{ "ls"      , &cmd_ls       },
        { "lua_e"   , &cmd_lua_e    },
#ifndef NYADOS
	{ "open"    , &cmd_open     },
#endif
	{ "option"  , &cmd_option   },
	{ "popd"    , &cmd_popd     },
	{ "print"   , &cmd_echoOut  },
	{ "printerr", &cmd_echoErr  },
	{ "pushd"   , &cmd_pushd    },
	{ "pwd"     , &cmd_pwd      },
	{ "set"	    , &cmd_set      },
	{ "shift"   , &cmd_shift    },
	{ "source"  , &cmd_source   },
	{ "sub"     , &cmd_sub      },
	{ "suffix"  , &cmd_suffix   },
	{ "unalias" , &cmd_unalias  },
	{ "unoption", &cmd_unoption },
	{ "unsuffix", &cmd_unsuffix },
	{ "xptest"  , &cmd_xptest },
	{ 0 , 0 },
    };
    /* �ÓI�����o�ϐ�����x�������������� */
    static int flag=1;
    if( flag ){
	flag = 0;
	for( const struct Command *ptr=cmdlist ; ptr->name != NULL ; ++ptr ){
	    NnString key( ptr->name );
	    key.downcase();
	    command.put( key , new NyadosCommand(ptr->proc) );

	    key.insertAt(0,"__");
	    key << "__";
	    command.put( key , new NyadosCommand(ptr->proc) );
	}
    }
    parent_ = parent ;
    out_    = new AnsiConsoleWriter( stdout ) ;
    err_    = new AnsiConsoleWriter( stderr ) ;
    in_     = new StreamReader( stdin  ) ;
}
NnHash NyadosShell::command;

NyadosShell::~NyadosShell(){}
int NyadosShell::setLabel( NnString & ){ return -1; }
int NyadosShell::goLabel( NnString & ){ return -1; }
int NyadosShell::argc() const { return 0; }
const NnString *NyadosShell::argv(int) const { return NULL; }
void NyadosShell::shift(){}
