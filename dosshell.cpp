#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "ntcons.h"
#include "nnhash.h"
#include "getline.h"
#include "nndir.h"
#include "writer.h"

/* ���̃n�b�V���e�[�u���ɁA�g���q���o�^����Ă���ƁA
 * ���̊g���q�̃t�@�C�����́A���s�\�ƌ��Ȃ����B
 * (��{�I�ɓo�^���e�́A������ => ������ ���O��)
 * �Ȃ��A�g���q�͏������ɕϊ����Ċi�[���邱�ƁB
 */
NnHash DosShell::executableSuffix;

/* ���̃t�@�C�������f�B���N�g�����ǂ����𖖔��ɃX���b�V���Ȃǂ�
 * ���Ă��邩�ǂ����ɂ�蔻�肷��B
 *      path - �t�@�C����
 * return
 *      ��0 - �f�B���N�g�� , 0 - ��ʃt�@�C��
 */
static int isDirectory( const char *path )
{
    int lastroot=NnDir::lastRoot(path);
    return lastroot != -1 && path[lastroot+1] == '\0';
}

/* ���̃t�@�C���������s�\���ǂ������g���q��蔻�肷��B
 *      path - �t�@�C����
 * return
 *      ��0 - ���s�\ , 0 - ���s�s�\
 */
static int isExecutable( const char *path )
{
    const char *suffix=strrchr(path,'.');
    /* �g���q���Ȃ���΁A���s�s�\ */
    if( suffix == NULL || suffix[0]=='\0' || suffix[1]=='\0' )
        return 0;
    
    /* �������� */
    NnString sfxlwr(suffix+1);
    sfxlwr.downcase();

    return sfxlwr.compare("com") == 0
        || sfxlwr.compare("exe") == 0
        || sfxlwr.compare("bat") == 0
	|| sfxlwr.compare("cmd") == 0
        || DosShell::executableSuffix.get(sfxlwr) != NULL ;
}

/* �R�}���h���⊮�̂��߂̌�⃊�X�g���쐬����B
 *      region - ��⊮������͈̔�
 *      array - �⊮��������ꏊ
 * return
 *      ���̐�
 */
int DosShell::makeTopCompletionList( const NnString &region , NnVector &array )
{
    NnString pathcore;

    /* �擪�̈��p�������� */
    if( region.at(0) == '"' ){
        pathcore = region.chars() + 1;
    }else{
        pathcore = region;
    }

    makeCompletionList( region , array );
    for(int i=0 ; i<array.size() ; ){
        NnString *fname=(NnString *)((NnPair*)array.at(i))->first();
        if( isExecutable( fname->chars())  ||  isDirectory( fname->chars() )){
            ++i;
        }else{
            array.deleteAt( i );
        }
    }

    if( region.findOf("/\\") != -1 )
        return array.size();
    
    const char *path=getEnv("PATH",NULL);
    if( path == NULL )
        return array.size();

    /* ���ϐ� PATH �𑀍삷�� */
    NnString rest(path);
    while( ! rest.empty() ){
	NnString path1;

	rest.splitTo(path1,rest,";");
	if( path1.empty() )
	    continue;
	path1.dequote();
        path1 += "\\*";
        for( NnDir dir(path1) ; dir.more() ; dir.next() ){
            if(    ! dir.isDir()
                && dir->istartsWith(pathcore) 
                && isExecutable(dir.name()) )
            {
                if( array.append(new NnPair(new NnStringIC(dir.name()))) )
                    break;
            }
        }
        /* �G�C���A�X�����ɂ䂭 : (����)�O���[�o���ϐ����Q�Ƃ��Ă��� */
        extern NnHash aliases;
        for( NnHash::Each p(aliases) ; p.more() ; p.next() ){
            if( p->key().istartsWith(pathcore) ){
                if( array.append( new NnPair(new NnStringIC(p->key()) )) )
                    break;
            }
        }
    }
    array.sort();
    array.uniq();
    return array.size();
}

void DosShell::putchr(int ch)
{
    conOut << (char)ch;
}

void DosShell::putbs(int n)
{
    while( n-- > 0 )
	conOut << (char)'\b';
}

int DosShell::getkey()
{
    fflush(stdout);
#ifdef NYACUS
    conOut << "\x1B[>5l";
#endif

    int ch=Console::getkey();
    if( isKanji(ch) ){
#if 1
	if( ch ==0xE0 ) /* xscript */
	    ch = 0x01;
#endif
        ch = (ch<<8) | Console::getkey() ;
    }else if( ch == 0 ){
        ch = 0x100 | Console::getkey() ;
    }
    return ch;
}

/* �ҏW�J�n�t�b�N */
void DosShell::start()
{
    prompt();
}

/* �ҏW�I���t�b�N */
void DosShell::end()
{
    putchar('\n');
}

#ifdef PROMPT_SHIFT
/* �v�����v�g�̂����An�J�����ڂ�������o��
 *      prompt - ��������
 *      offset - ���o���ʒu
 *      result - ���o��������
 * return
 *      ���o�v�����v�g�̌���
 */
static int prompt_substr( const NnString &prompt , int offset , NnString &result )
{
    int i=0;
    int column=0;
    int nprints=0;
    int knj=0;
    for(;;){
        if( prompt.at(i) == '\x1B' ){
            for(;;){
                if( prompt.at(i) == '\0' )
                    goto exit;
                result << (char)prompt.at(i);
                if( isAlpha(prompt.at(i++)) )
                    break;
            }
        }else if( prompt.at(i) == '\0' ){
            break;
        }else{
            if( column == offset ){
                if( knj )
                    result << ' ';
                else
                    result << (char)prompt.at(i);
                ++nprints;
            }else if( column > offset ){
                result << (char)prompt.at(i);
                ++nprints;
            }
            if( knj == 0 && isKanji(prompt.at(i)) )
                knj = 1;
            else
                knj = 0;
            ++i;
            column++;
        }
    }
  exit:
    return nprints;
}


#else
/* �G�X�P�[�v�V�[�P���X���܂܂Ȃ��������𓾂�. 
 *      p - ������擪�|�C���^
 * return
 *      ������̂����A�G�X�P�[�v�V�[�P���X���܂܂Ȃ����̒���
 *      (�����Ȍ���)
 */
static int strlenNotEscape( const char *p )
{
    int w=0,escape=0;
    while( *p != '\0' ){
        if( *p == '\x1B' )
            escape = 1;
        if( ! escape )
            ++w;
        if( isAlpha(*p) )
            escape = 0;
	if( *p == '\n' )
	    w = 0;
        ++p;
    }
    return w;
}
#endif

/* �v�����v�g��\������.
 * return �v�����v�g�̌���(�o�C�g���ł͂Ȃ�=>�G�X�P�[�v�V�[�P���X���܂܂Ȃ�)
 */
int DosShell::prompt()
{
    const char *sp=prompt_.chars();
    NnString prompt;

    time_t now;
    time( &now );
    struct tm *thetime = localtime( &now );
    
    if( sp != NULL && sp[0] != '\0' ){
        while( *sp != '\0' ){
            if( *sp == '$' ){
                ++sp;
                switch( toupper(*sp) ){
                    case '_': prompt << '\n';   break;
                    case '$': prompt << '$'; break;
		    case 'A': prompt << '&'; break;
                    case 'B': prompt << '|'; break;
		    case 'C': prompt << '('; break;
                    case 'D':
			prompt.addValueOf(thetime->tm_year + 1900);
			prompt << '-';
			if( thetime->tm_mon + 1 < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_mon + 1);
			prompt << '-';
			if( thetime->tm_mday < 10 )
			    prompt << '0';
			prompt.addValueOf( thetime->tm_mday );
                        switch( thetime->tm_wday ){
                            case 0: prompt << " (��)"; break;
                            case 1: prompt << " (��)"; break;
                            case 2: prompt << " (��)"; break;
                            case 3: prompt << " (��)"; break;
                            case 4: prompt << " (��)"; break;
                            case 5: prompt << " (��)"; break;
                            case 6: prompt << " (�y)"; break;
                        }
                        break;
                    case 'E': prompt << '\x1B'; break;
		    case 'F': prompt << ')'; break;
                    case 'G': prompt << '>';    break;
                    case 'H': prompt << '\b';   break;
                    case 'I': break;
                    case 'L': prompt << '<';    break;
		    case 'N': prompt << (char)NnDir::getcwdrive(); break;
                    case 'P':{
                        NnString pwd;
                        NnDir::getcwd(pwd) ;
                        prompt << pwd; 
                        break;
                    }
                    case 'Q': prompt << '='; break;
		    case 'S': prompt << ' ';    break;
                    case 'T':
			if( thetime->tm_hour < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_hour);
			prompt << ':';
			if( thetime->tm_min < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_min);
			prompt << ':';

			if( thetime->tm_sec < 10 )
			    prompt << '0';
			prompt.addValueOf(thetime->tm_sec) ;
                        break;
                    case 'V':
			prompt << SHELL_NAME ; break;
                    case 'W':{
                        NnString pwd;
                        NnDir::getcwd(pwd);
			int rootpos=pwd.findLastOf("/\\");
			if( rootpos == -1 || rootpos == 2 ){
                            prompt << pwd;
			}else{
                            prompt << (char)pwd.at(0) << (char)pwd.at(1);
                            prompt << pwd.chars()+rootpos+1;
			}
			break;
                    }
                    default:  prompt << '$' << *sp; break;
                }
                ++sp;
            }else{
                prompt += *sp++;
            }
        }
    }else{
        NnString pwd;
        NnDir::getcwd(pwd);
        prompt << pwd << '>';
    }

#ifdef PROMPT_SHIFT
    NnString prompt2;
    prompt_size = prompt_substr( prompt , prompt_offset , prompt2 );
    conOut << prompt2;
#else
    int prompt_size = strlenNotEscape( prompt.chars() );
    conOut << prompt;
#endif

    /* �K���L�[�v���Ȃ��Ă͂����Ȃ��ҏW�̈�̃T�C�Y���擾���� */
    NnString *temp;
    int minEditWidth = 10;
    if(    (temp=(NnString*)properties.get("mineditwidth")) != NULL
        && (minEditWidth=atoi(temp->chars())) < 1 )
    {
        minEditWidth = 10;
    }

    int whole_width=DEFAULT_WIDTH;
    if(    (temp=(NnString*)properties.get("width")) != NULL
        && (whole_width=atoi(temp->chars())) < 1 )
    {
        whole_width = DEFAULT_WIDTH;
    }
#ifdef NYACUS
    else {
        whole_width = Console::getWidth();
    }
#endif

    int width=whole_width - prompt_size % whole_width ;
    if( width <= minEditWidth ){
	conOut << '\n';
        width = whole_width;
    }
    setWidth( width );
    return width;
}

void DosShell::clear()
{
    conOut << "\x1B[2J";
}
