/* ntconsole.cpp
 *   �R���\�[���𒼐ڑ��삷��֐��Ȃǂ��i�[����B
 */

#include "config.h"
#if !defined(__DMC__) && !defined(__OS2__)
#  include <conio.h>
#endif
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(NYADOS)
#  include <dos.h>
#endif
#include "ntcons.h"
#include "nnstring.h"

#if defined(__EMX__) /*** OS/2 ��p static �ϐ�/�֐��Q ***/

#include <sys/kbdscan.h>
#define INCL_WIN
#define INCL_VIO
#define INCL_DOSPROCESS
#include <os2.h> 

static HAB hab,hmq;

static int init_os2()
{
    static int first=1;
    if( first ){
	first = 0;

	PTIB  ptib = NULL;
	PPIB  ppib = NULL;
	APIRET rc = DosGetInfoBlocks(&ptib, &ppib);
	if (rc != 0)
	    return rc;
	ppib->pib_ultype = PROG_PM;
	hab = WinInitialize(0);
	hmq = WinCreateMsgQueue(hab, 0);
	if (hmq == NULLHANDLE)
	    return rc;
    }
    return 0;
}

#elif defined(NYACUS) /*** Borland C++/VC++ ��p static �ϐ�/�֐��Q ***/

#include <windows.h>

int getch_replacement_for_msvc();

static HANDLE   hStdin = (HANDLE )-1L;
static HANDLE   hStdout = (HANDLE )-1L;
static BOOL     bStdinIsConsole;

static DWORD    default_console_mode = ~0u;

/* �ύX���ꂽ�W�����o�͂����ɖ߂� */
void Console::restore_default_console_mode()
{
    if( default_console_mode != ~0u ){
        SetConsoleMode( GetStdHandle(STD_INPUT_HANDLE) , 
                        default_console_mode );
    }
}

/* API�ɂ��W�����o�͂̏����� */
static void initializeStdio()
{
    static bool firstcalled=true;

    hStdin  = GetStdHandle(STD_INPUT_HANDLE);   /* �W�����̓n���h���̎擾 */
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);  /* �W���o�̓n���h���̎擾 */
    if( firstcalled ){
        firstcalled = false;

        DWORD dw;
        /* stdin ���_�C���N�g���̑Ώ��istdin��unbuffered mode �Ɂj*/
        setvbuf(stdin, NULL, _IONBF, 0);
        setmode(fileno(stdin), O_BINARY);
        if (GetConsoleMode(hStdin, &dw)) {
            /* Win32�R���\�[���̏ꍇ�͔O�̂��߃_�C���N�g���[�h�ɐݒ� */
            bStdinIsConsole = TRUE;
            if( default_console_mode == ~0u ){
                default_console_mode = dw;
                atexit( Console::restore_default_console_mode );
            }
        }
    }
}

void Console::enable_ctrl_c()
{
    initializeStdio();
    SetConsoleMode(hStdin, default_console_mode );
}

void Console::disable_ctrl_c()
{
    initializeStdio();
    SetConsoleMode(hStdin, default_console_mode 
                              & ~ENABLE_PROCESSED_INPUT 
                              & ~ENABLE_LINE_INPUT
                              & ~ENABLE_ECHO_INPUT
                              & ~ENABLE_INSERT_MODE
                              & ~ENABLE_QUICK_EDIT_MODE);
}

void Console::getLocate(int &x,int &y)
{
    if (hStdout == (HANDLE )-1L)
	initializeStdio();

    CONSOLE_SCREEN_BUFFER_INFO  csbi;
    if (GetConsoleScreenBufferInfo(hStdout,&csbi) == FALSE)
	return;
    
    x = csbi.dwCursorPosition.X;
    y = csbi.dwCursorPosition.Y;
}

#else /***** DOS �p static �֐�/�ϐ��Q ****/
static NnString tinyClipBoard;
#endif

/* �R���\�[���̃N���A */
void  Console::clear()
{
#if defined(ESCAPE_SEQUENCE_OK)
    fputs("\x1B[2J",stdout);
#else
# if defined(NYACUS)
    static COORD                coordScreen;
    DWORD                       dwCharsWritten;
    DWORD                       dwConsoleSize;
    CONSOLE_SCREEN_BUFFER_INFO  csbi;

    if (hStdout == (HANDLE )-1L)
	initializeStdio();

    /* �R���\�[���̃L�����N�^�o�b�t�@�����擾 */
    if(GetConsoleScreenBufferInfo(hStdout,&csbi) == FALSE)
	initializeStdio();

    /* �L�����N�^�o�b�t�@�T�C�Y���v�Z */
    dwConsoleSize = csbi.dwSize.X * csbi.dwSize.Y;

    /* �L�����N�^�o�b�t�@���󔒂Ŗ��߂� */
    FillConsoleOutputCharacter(
        hStdout,' ',dwConsoleSize,coordScreen,&dwCharsWritten);

    /* ���݂̃e�L�X�g�����̎擾 */
    if (GetConsoleScreenBufferInfo(hStdout,&csbi) == FALSE)
        return;

    /* ���ׂĂ̕����ɑ΂��Ď擾�����e�L�X�g������K�p���� */
    FillConsoleOutputAttribute(
        hStdout,csbi.wAttributes,dwConsoleSize,coordScreen,&dwCharsWritten);

    /* �J�[�\���ʒu������p�Ɉړ� */
    SetConsoleCursorPosition(hStdout,coordScreen);
# else
#   error Write Platform specific Console::clear()!
# endif
#endif
}

void Console::backspace(int n)
{
    if( n <= 0 )
	return;

#ifdef ESCAPE_SEQUENCE_OK
    printf("\x1B[%dD",n);
#elif defined(NYACUS)
    int x=0,y=0;
    getLocate(x,y);
    locate(x-n,y);
#else
    while( n-- > 0 )
	putchar('\b');
#endif
}

#ifdef NYACUS
void Console::color(int n)
{
    if (hStdout == (HANDLE )-1L)
	initializeStdio();
    SetConsoleTextAttribute(hStdout,n);
}


int Console::foremask(int n)
{
    return n & (
	FOREGROUND_RED
	|FOREGROUND_BLUE
	|FOREGROUND_GREEN
	|FOREGROUND_INTENSITY);
}


int Console::backmask(int n)
{
    return n & (
	BACKGROUND_RED
	|BACKGROUND_BLUE
	|BACKGROUND_GREEN
	|BACKGROUND_INTENSITY);
}


int Console::color()
{
    if (hStdout == (HANDLE )-1L)
	initializeStdio();

    CONSOLE_SCREEN_BUFFER_INFO  csbi;
    if (GetConsoleScreenBufferInfo(hStdout,&csbi) == FALSE)
	return -1;

    return csbi.wAttributes;
}


int Console::getWidth()
{
    CONSOLE_SCREEN_BUFFER_INFO  csbi;
    if (hStdout == (HANDLE )-1L)
	initializeStdio();

    if(GetConsoleScreenBufferInfo(hStdout,&csbi) == FALSE)
	initializeStdio();

    return (int)csbi.dwSize.X;
}

#endif

static int getkey_()
{
#ifdef __DMC__
#ifdef ASM_OK
    _asm {
	mov ah,07h
	int 21h
	xor ah,ah
    }
#else
    union REGS in,out;
    in.h.ah = 0x7;
    int86( 0x21 , &in , &out );
    return out.h.al & 255;
#endif
#elif defined(__EMX__)
    return _read_kbd(0,1,0);
#elif defined(NYACUS)
    int c;
    if (bStdinIsConsole) {
        c = getch_replacement_for_msvc();
    } else {
        /* stdin �����_�C���N�g����Ă���ꍇ�i���ƓK���j */
        c = fgetc(stdin);
        if (c == '\n') c = '\r';
        if (c == EOF) exit(0);
    }
    return c & 255;
#else
    return getch() & 255;
#endif
}

int Console::getkey()
{
    int ch=getkey_();
    if( isKanji(ch) ){
#if 0
        /* ���� MSVCRT.DLL ���g���ꍇ�A����L�[��
         * 0xE0 �Ƃ����v���t�B�b�N�X�ŗ^�����Ă��܂��̂ŁA
         * ���̂悤�ȗ�O��݂��Ȃ��Ă͂����Ȃ������B
         * 
         * ���݂́ALukewarm����̎��O getch �𗘗p���Ă���̂ŁA
         * �����͉�������Ă���B
         * (�v���t�B�b�N�X�� 0 �ɂȂ��Ă���)
         */
	if( ch ==0xE0 ) /* xscript */
	    ch = 0x01;
#endif
        ch = (ch<<8) | getkey_() ;
    }else if( ch == 0 ){
        ch = 0x100 | getkey_() ;
    }
    return ch;
}

void Console::writeClipBoard( const char *ptr , int size )
{
#ifdef NYACUS
    HGLOBAL hText = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, size+1 );
    char *pText = (char*)GlobalLock(hText);
    if( pText != NULL ){
        memcpy( pText , ptr , size );
        pText[ size ] = '\0';
        GlobalUnlock(hText);

        OpenClipboard(NULL);
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hText);
        CloseClipboard();
    }
#elif defined(__EMX__)
    char *pText = NULL;
    DosAllocSharedMem( (void**)(&pText)
			, NULL
			, (ULONG)size+1
			, PAG_COMMIT | PAG_READ | PAG_WRITE
			| OBJ_TILE   | OBJ_GIVEABLE );
    if( pText != NULL ){
        memcpy( pText , ptr , size );
        pText[ size ] = '\0';
	init_os2();
	WinOpenClipbrd(hab);
	WinSetClipbrdOwner(hab,NULLHANDLE);
	WinEmptyClipbrd(hab);
	WinSetClipbrdData( hab, (ULONG)pText, CF_TEXT, CFI_POINTER );
	WinCloseClipbrd(hab);
    }
#else
    tinyClipBoard.assign(ptr,size);
#endif
}

void Console::readClipBoard( NnString &buffer )
{
#ifdef NYACUS
    buffer.erase();
    ::OpenClipboard(NULL);
    HANDLE hText=::GetClipboardData(CF_TEXT);
    if( hText != NULL ){
        char *pText = (char*)::GlobalLock(hText);
        if( pText != NULL ){
	    buffer = pText;
            ::GlobalUnlock(hText);
        }
    }
    ::CloseClipboard();
#elif defined(NYAOS2)
    init_os2();
    WinOpenClipbrd(hab);
    WinSetClipbrdOwner(hab,NULLHANDLE);
    ULONG ulFmtInfo;
    if( WinQueryClipbrdFmtInfo( hab, CF_TEXT, &ulFmtInfo ) ){
	char *text = (char*)WinQueryClipbrdData( hab, CF_TEXT );
	if( text != NULL )
	    buffer = text;
    }
    WinCloseClipbrd(hab);
#else
    buffer = tinyClipBoard;
#endif
}

#ifdef NYACUS
void Console::readTextVram( int x , int y , char *buffer , int n )
{
#ifdef NYACUS
    if (hStdout == (HANDLE )-1L)
	initializeStdio();
    COORD cursor={ x , y };
    DWORD size;
    ::ReadConsoleOutputCharacter( hStdout, buffer , n , cursor , &size );
#else
    union REGS in , out ;
    struct SREGS sregs;
    char *buffer1=new char[n*2];
    unsigned off1=FP_OFF(buffer1) , seg1 = FP_SEG(buffer1);

    _asm {
	push es
	push bp 

	mov  cx,n
	mov  dl,byte ptr x
	mov  dh,byte ptr y
	xor  bh,bh
	mov  ax,seg1
	mov  es,ax
	mov  bp,off1
	mov  ax,1310h
	int  010h

	pop  bp
	pop  es
    };
    /* 
	sregs.es = FP_SEG(buffer1);
	in.x.bp  = FP_OFF(buffer1);
	in.x.ax = 0x1310;
	in.x.cx = n;
	in.h.dl = x;
	in.h.dh = y;
	in.h.bh = 0;
    */

    int86x( 0x10 , &in , &out , &sregs );
    for(int i=0 ; i<n ; i++){
	buffer[i] = ( buffer1[i*2] ? buffer1[i*2] : ' ');
    }
    delete[]buffer1;
    buffer[n] = '\0';
#endif
}

void Console::locate(int x,int y)
{
#ifdef NYACUS
    COORD coord;
    if( hStdout == (HANDLE )-1L )
	initializeStdio();

    coord.X = x ; coord.Y = y;
    SetConsoleCursorPosition(hStdout,coord);
#else
    _asm{
	mov ah,02h
	xor bh,bh
	mov dl,byte ptr x
	mov dh,byte ptr y
	int 10h
    }
#endif
}
unsigned int Console::getShiftKey()
{
#ifdef NYACUS
    return GetKeyState( VK_SHIFT ) ;
#else
    unsigned char result;
    _asm{
	mov ah,02h
	int 16h
	mov result,al
    }
    return result;
#endif
}
#endif

void Console::setConsoleTitle( const char *s )
{
#ifdef NYACUS
    SetConsoleTitle( s );
#endif
}
