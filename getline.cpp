#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#undef  CTRL
#define CTRL(x) ((x) & 0x1F)

#include "getline.h"
#include "nnhash.h"
#include "nua.h"

History GetLine::history;

Status (GetLine::*GetLine::which_command(int key))(int key)
{
    return (unsigned)key>=numof(bindmap) 
        ? &GetLine::insert 
        : bindmap[(unsigned)key];
}
Status GetLine::interpret(int key)
{
    return (this->*this->which_command(key))(key);
}

/* �o�b�t�@��̂���͈͂�\������B
 * �����Ŕ{�p���������f�����Ƃ��̓_�~�[�������\�������B
 *      from - �\���J�n�ʒu(�o�b�t�@��ł̈ʒu)
 *      to   - �\���I���ʒu(�o�b�t�@��ł̈ʒu)
 * [from,to-1]���\�������B
 * return
 *      �\�����������̃o�C�g��(����)
 */
int GetLine::puts_between(int from,int to)
{
    if( from <  offset )         from = offset;
    if( to   >  offset+width )   to   = offset+width;

    /* 1������ */
    if( to-from > 0 )
        putchr( buffer.isKnj2(from) ? '^' : buffer[ from ] );

    /* 2�����ځ`n-1������ */
    for(int i=from+1 ; i < to-1 ; i++)
        putchr( buffer[i] );

    /* n������ */
    if( to-from > 1 )
        putchr( buffer.isKnj1(to-1) ? '$' : buffer[to-1] );
    
    return to-from <= 0 ? 0 : to-from ;
}

int GetLine::putspc(int n)
{
    for(int i=0;i<n;i++)
        putchr(' ');
    return n;
}


/* �J�[�\���ȍ~���ĕ`�� */
void GetLine::repaint_after(int rm)
{
    int bs=puts_between(pos,buffer.length());
    while( rm-- > 0  && (pos+bs-offset) < width ){
        putchr( ' ' );
        ++bs;
    }
    putbs( bs );
}


/* �J�[�\���ʒu�ɕ������}�����A�\�����X�V����B
 *      s - ������
 * return
 *      �}���o�C�g��
 */
int GetLine::insertHere( const char *s )
{
    int n=buffer.insert(s,pos);
    if( n <= 0 )
        return 0; // memory allocation error.

    if( pos+n < offset+width ){
        // �}��������̖�������ʕ��Ɏ��܂�ꍇ�A���̂܂ܕ\������.
        pos += puts_between( pos , pos+n );
        repaint_after();
    }else{
        /* �}��������̖�������ʕ��𒴂���ꍇ
         * �擪���v�Z���Ȃ����āA�S�̂�\�����Ȃ����B
         */
        putbs( pos-offset );
        offset = (pos+=n)-width;
        puts_between( head() , pos );
    }
    return n;
}

/* ��s���̓��C���֐�(1).
 *	result - [in] ���͂̃f�t�H���g�l [out] ���͌���
 * return
 *	0�ȏ� - ������̒���
 *	-1    - �L�����Z�����ꂽ
 *	-2    - ���̑��̃G���[
 */
Status GetLine::operator() ( NnString &result )
{
    if( buffer.init() )
        return TERMINATE;

    pos = offset = 0;
    width = DEFAULT_WIDTH ;

    history_pointor = history.size();
    NnString foo;
    history << foo;

    start(); /* �� �v�����v�g���\������� */

    /* �f�t�H���g�l������ꍇ */
    if( result.length() > 0 ){
        if( buffer.insert( result.chars(),0 ) > 0 ){
            // �}�������������ꍇ�̂�:
            if( (pos=buffer.length()) >= width )
                offset = buffer.length()-width;
        
            puts_between( offset , buffer.length() );
        }
    }

    lastKey = 0;
    
    for(;;){
        int key=getkey();
        Status rc=CONTINUE;
#ifdef LUA_ENABLE
        NyaosLua L(NULL); /* stacked [nyaos] */
        if( L.ok() ){
            lua_getfield(L,-1,"keyhook"); /* [nyaos,keyhook] */
            if( lua_type(L,-1) == LUA_TFUNCTION ){
                int start=lua_gettop(L);  /* [nyaos,keyhook] */

                lua_newtable(L);
                lua_pushinteger(L,key);
                lua_setfield(L,-2,"key");
                lua_pushinteger(L,pos);
                lua_setfield(L,-2,"pos");
                lua_pushstring(L,buffer.chars());
                lua_setfield(L,-2,"text"); /* [nyaos,keyhook,T] */

                if( lua_pcall(L,1,LUA_MULTRET,0) == 0 ){
                    /* keyhook �܂ł� pop ���� [nyaos,r1,r2...] */
                    int n=lua_gettop(L);
                    if( n < start ){ /* ���� 0 �� */
                        rc = interpret( key );
                    }else{
                        for(int i=start ; i<=n && rc==CONTINUE ;i++){
                            switch( lua_type(L,i) ){
                            default:
                            case LUA_TNIL:
                                rc = interpret( key );
                                break;
                            case LUA_TBOOLEAN:
                                rc = lua_toboolean(L,i) ? ENTER : CANCEL;
                                break;
                            case LUA_TNUMBER:
                                rc = interpret( lua_tointeger(L,i) );
                                break;
                            case LUA_TSTRING:
                                this->insertHere( lua_tostring(L,i) );
                                break;
                            }
                        }
                    }
                }else{
                    /* messaging error */
                    /* keyhook �܂ł� pop ���� [nyaos,message] */
                    putchr('\n');
                    const char *p=lua_tostring(L,-1);
                    while( p != NULL && *p != '\0' ){
                        putchr(*p++);
                    }
                    putchr('\n');
                    prompt();
                    puts_between( offset , pos );
                    repaint_after();
                }
            }else{
                rc = interpret(key);
            }
        }else{
#endif
            rc = interpret(key);
#ifdef LUA_ENABLE
        }
#endif
	int len;
        switch( rc ){
        case CONTINUE:
            break;

        case TERMINATE:
        case CANCEL:
            buffer.term();
            end();
            result.erase();
            history.drop();
            return rc;
          
        case ENTER:
            if( buffer.length() > 0 ){
		len = buffer.decode( result );
		NnString *r=history.top();
		if( r != NULL )
		    *r = result;
                history.pack();
            }else{
		len = 0;
                history.drop();
                result = "";
            }
            end();
            buffer.term();
            return rc;
        }
        lastKey = key;
    }
}

void GetLine::replace_all_repaint( const NnString &s )
{
    // �v�����v�g����ɃJ�[�\����߂�.
    putbs( pos-offset );

    // ����܂ł̕\�����������J�E���g.
    int clearSize=printedSize();

    // ���݂̃q�X�g���J�[�\���̓��e��ҏW���e�ƒu��������.
    buffer.replace( 0,buffer.length(),s.chars() );

    pos = buffer.length();
    offset = 0;

    // �\�����������������傫����΁A�\���g�b�v�����炷.
    if( pos+1 >= width )
        offset = pos+1-width;
    
    puts_between( offset , buffer.length() );

    int diff = clearSize - printedSize() ;
    for( int bs=0 ; bs < diff ; bs++ )
        putchr(' ');
    putbs(diff);
}

/* �P��𒊏o����
 *      m - �s�ʒu
 *      n - �ꐔ�ʒu
 *      word - �����P��
 */
void GetLine::get_nline_nword(int m,int n,NnString &word)
{
    NnString line=*history[m];
    NnString left;

    line.splitTo( word , left);
    for(int i=0 ; i< n ; i++ ){
        left.splitTo( word , left );
    }
}

/* �s�P�ʂŃq�X�g������������
 *      m - �q�X�g���ԍ�
 *      line - �����L�[
 */
int GetLine::seekLineForward(int &m, const char *line)
{
    for(;;){
        if( --m < 0 )
            return -1;
        if( history[m]->startsWith(line) )
            return 0;
    }
}
/* �s�P�ʂŃq�X�g������������
 *      m - �q�X�g���ԍ�
 *      line - �����L�[
 */
int GetLine::seekLineBackward(int &m, const char *line )
{
    for(;;){
        if( ++m >= history.size() )
            return -1;
        if( history[m]->startsWith(line) )
            return 0;
    }
}

/* �P�����������
 *      m - �s�ʒu
 *      n - �ꐔ�ʒu
 *      word - �����P��
 * return
 *      0 - ��������
 *      -1 - ������Ȃ�����
 */
int GetLine::seekWordForward(int &m,int &n,const NnString &word , NnString &found)
{
    for(;;){
        get_nline_nword(m,++n,found);
        if( found.empty() ){ /* �����Ɉړ� */
            if( --m < 0 )
                return -1;
            n=0;
        }else if( found.startsWith(word) ){
            return 0;
        }
    }
}
static int count_words(const NnString &line)
{
    int i=0;
    int quote=0;
    int nwords=0;
    for(;;){
        for(;;){
            if( i >= line.length() )
                return nwords;
            if( ! isSpace(line.at(i)) )
                break;
            ++i;
        }
        ++nwords;
        for(;;){
            if( i >= line.length() )
                return nwords;
            if( !quote && isSpace(line.at(i)) )
                break;
            if( line.at(i)=='"')
                quote = !quote;
            ++i;
        }
    }
}

/* ���������֒P�����������
 *      m - �s�ʒu
 *      n - �ꐔ�ʒu
 *      word - �����P��
 * return
 *      0 - ��������
 *      -1 - ������Ȃ�����
 */
int GetLine::seekWordBackward(int &m,int &n,const NnString &word,NnString &found)
{
    for(;;){
        while( --n <= 0 ){
            if( ++m >= history.size() )
                return -1;
            n = count_words(*history[m]);
        }
        get_nline_nword(m,n,found);
        if( found.startsWith(word) )
            return 0;
    }
}

/* printSeal �ŕ\�������V�[�������� */
void GetLine::eraseSeal( int sealsize )
{
    putbs( sealsize );
    if( offset > pos ){
	offset = pos;
    }
    repaint_after(sealsize);
}

/* �J�[�\���ʒu����V�[����\������B
 * �V�[���� repaint_after �����s���邱�Ƃŏ����ł���B
 *      seal �\������V�[��
 *      sealsize  �O��̕\�������V�[���T�C�Y
 *           (�㏑���ł��Ȃ������������󔒂ɂ���)
 * return
 *      �V�[���T�C�Y
 */
int GetLine::printSeal( const char *seal , int sealsize )
{
    putbs( sealsize );
    int seallen = TwinBuffer::strlen_ctrl(seal);
    if( width+offset < pos + seallen ){
        /* �V�[����\�������Ԃ��Ȃ��ꍇ�́A
         * ���֓K���ɃX�N���[��������B
         *    ���X offset < pos < offset+width < pos + seallen
         */
        putbs( pos - offset );
        offset = pos + seallen - width;
        if( offset < 0 )
            offset = 0;
        puts_between( offset , pos );
    }
    int bs=0;
    int i=pos;
    while( *seal != '\0' ){
	if( TwinBuffer::isCtrl(*seal) ){
	    if( bs+1 >= width || i+1 >= offset+width )
		break;
	    putchr( '^' );
	    putchr( '@' + *seal++ );
	    bs += 2;
	    i  += 2;
	}else{
	    if( bs >= width || i >= offset+width)
		break;
	    putchr( *seal++ );
	    ++bs;
	    ++i;
	}
    }
    int bs2=bs;
    while( i < offset+width  &&  bs2 < sealsize && bs2 < width ){
        putchr( ' ' );
        ++bs2;
        ++i;
    }
    if( bs2 > bs )
        putbs( bs2-bs );
    return bs;
}

GetLine::~GetLine(){}

int GetLine::prompt(){ return 0;}

/* �J�[�\����̒P��͈͎̔擾.
 * out:
 *	at - �P��̐擪
 *	size - �P��̒���
 * return:
 *	�����񎩐g
 */
NnString GetLine::current_word(int &at,int &size)
{
    NnString *temp=(NnString *)properties.get("uncompletechar");
    const char *dem="";
    if( temp != NULL ){
	temp->trim();
	dem = temp->chars();
    }

    int inQuote=0;
    int j=0;
    at = 0;
    for(;;){
        for(;;){ /* �󔒂̃X�L�b�v */
            if( j >= pos ){
		/* ������ goto done �ł͂Ȃ��Aat ��ύX������ׂ��A
		 * break �ɂ��Ă����Ȃ��Ă͂����Ȃ� */
		break;
	    }
            if( ! isSpace(buffer[j] & 255) && strchr(dem,buffer[j] & 255) == NULL )
                break;
            ++j;
        }
        at = j;
        for(;;){
            if( j >= pos )
                goto done;
            if( !inQuote ){
                if( isSpace(buffer[j]) || strchr(dem,buffer[j]) != NULL )
                    break;
            }
            if( buffer[j] == '"' )
                inQuote = !inQuote;
            ++j;
        }
    }
  done:
    size = j - at;
  
    NnString result;
    buffer.decode(at,size,result);
    return result;
}

