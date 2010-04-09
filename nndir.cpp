#include "config.h"

#if defined(NYADOS)
#  include <dos.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef NYADOS
#  include <fcntl.h>
#  include <io.h>
#endif

#if defined(OS2EMX)
#  define INCL_DOSFILEMGR
#  include <os2.h>
#else
#  include <dir.h>
#endif

#include "nndir.h"

#if defined(__DMC__) && !defined(__OS2__)
#    undef FP_SEG
#    define FP_SEG(x) ((unsigned short)(((unsigned long)(x))>>16))
#    undef FP_OFF
#    define FP_OFF(x) ((unsigned short)(x))
#endif

/* Default Special Folder �ݒ�̂��� */
#if defined(NYACUS)
#    include <windows.h>
#    include <shlobj.h>
#    include <stdio.h>
#endif

int NnTimeStamp::compare( const NnTimeStamp &o ) const
{
    return year   != o.year   ? o.year    - year 
	:  month  != o.month  ? o.month   - month
	:  day    != o.day    ? o.day     - day
	:  hour   != o.hour   ? o.hour    - hour
	:  minute != o.minute ? o.minute  - minute
	                      :  o.second - second ;
}

#ifdef _MSC_VER
static void stamp_conv( const FILETIME *p , NnTimeStamp &stamp_)
{
    FILETIME local;
    SYSTEMTIME s;

    FileTimeToLocalFileTime( p , &local );
    FileTimeToSystemTime( &local , &s );
    stamp_.second = s.wSecond ;
    stamp_.minute = s.wMinute ;
    stamp_.hour   = s.wHour ;
    stamp_.day    = s.wDay ;
    stamp_.month  = s.wMonth ;
    stamp_.year   = s.wYear ;
}
#else

static void stamp_conv( unsigned fdate , unsigned ftime , NnTimeStamp &stamp_ )
{
    /* ���� */
    stamp_.second = ( ftime & 0x1F) * 2 ;      /* �b:5bit(0..31) */
    stamp_.minute = ( (ftime >> 5 ) & 0x3F );  /* ��:6bit(0..63) */
    stamp_.hour   =  ftime >> 11;              /* ��:5bit */

    /* ���t */
    stamp_.day    = (fdate & 0x1F);            /* ��:5bit(0..31) */
    stamp_.month = ( (fdate >> 5 ) & 0x0F );   /* ��:4bit(0..16) */
    stamp_.year   =  (fdate >> 9 ) + 1980;     /* �N:7bit */
}
#endif

static void stamp_conv( time_t time1 , NnTimeStamp &stamp_ )
{
    struct tm *tm1;

    tm1 = localtime( &time1 );
    stamp_.second = tm1->tm_sec  ;
    stamp_.minute = tm1->tm_min  ;
    stamp_.hour   = tm1->tm_hour ;
    stamp_.day    = tm1->tm_mday ;
    stamp_.month  = tm1->tm_mon + 1;
    stamp_.year   = tm1->tm_year + 1900 ;
}

enum{
    W95_DATE_FORMAT = 0 ,
    DOS_DATE_FORMAT = 1 ,
};
extern int read_shortcut(const char *src,char *buffer,int size);

/** �u...\�v���u..\..\�v DOS,OS/2 �̈� */
void NnDir::extractDots( const char *&sp , NnString &dst )
{
    ++sp;
    for(;;){
	dst << "..";
	if( *++sp != '.' )
	    break;
	dst << '\\';
    }
}


NnDir::~NnDir()
{
    NnDir::findclose();
}
void NnDir::operator++()
{
    status = NnDir::findnext();
}
/* �t�@�C��������΁A�t�@�C����������I�u�W�F�N�g�ւ̃|�C���^��Ԃ��B
 */
NnObject *NnDir::operator * ()
{
    return status ? 0 : &name_;
}

#ifdef NYADOS
/* �h���C�u�� LFN ���T�|�[�g���Ă��邩�𒲍�������B
 * return
 *	��0 : �T�|�[�g���Ă���.
 *      0 : �T�|�[�g���Ă��Ȃ�.
 */
int isLfnOk()
{
    static int  result=-1;
    static char	rootdir[] = "?:\\";
    static char filesys[10];

    union REGS in, out;
#ifdef USE_FAR_PTR
    struct SREGS segs;
#endif

    if( result != -1 )
	return result;

    /* �J�����g�h���C�u���擾����B*/
#ifdef ASM_OK
    _asm {
	mov ah,19h
	int 21h
	add ah,'a'
	mov rootdir[0],ah
    }
#else
    in.h.ah = 0x19;
    intdos(&in, &out);
    rootdir[0] = 'a' + out.h.al ;
#endif

    /* �h���C�u���𓾂� */
    in.x.ax   = 0x71A0;
    segs.ds   = FP_SEG(rootdir);
    segs.es   = FP_SEG(filesys);
    in.x.si   = FP_OFF(rootdir);
    in.x.di   = FP_OFF(filesys);
    in.x.cx   = sizeof(filesys);
    intdosx(&in,&out,&segs);

    return result=(out.x.ax != 0x7100 ? 1 : 0);
}

/* findfirst/findnext ���Ăԍۂ̋��ʕ���.
 *	in -> ���̓��W�X�^���
 *     out <- �o�̓��W�X�^���
 * return
 *	0 ... ����
 *	1 ... ���s(�Ō�̃t�@�C����������)
 *	-1 .. ���s(LFN���T�|�[�g����Ă��Ȃ�)
 */
#ifdef USE_FAR_PTR
unsigned NnDir::findcore( union REGS &in , union REGS &out , struct SREGS &segs )
#else
unsigned NnDir::findcore( union REGS &in , union REGS &out )
#endif
{
    struct { 
	unsigned long	attrib;
	unsigned short  ctime , cdate , dummy1 , dummy2 ;
	unsigned short  atime , adate , dummy3 , dummy4 ;
	unsigned short  mtime , mdate , dummy5 , dummy6 ;
	unsigned long	hsize, lsize;
	char	 	reserved[8];
	char	 	lname[260], name[14];
    } findbuf;

    memset( &findbuf , '\0' , sizeof(findbuf) );

#ifdef USE_FAR_PTR
    segs.es = FP_SEG( &findbuf );
#endif
    in.x.di = FP_OFF( &findbuf );

#ifdef USE_FAR_PTR
    unsigned result = intdosx(&in,&out,&segs);
#else
    unsigned result = intdos(&in,&out);
#endif
    if( result==0x7100 && isLfnOk()==0 )
	return -1;// AX�ɕω����Ȃ��ALFN���T�|�[�g����Ă��Ȃ��ꍇ�A�G���[.

    if( out.x.cflag )
	return 1;// �L�����[�t���O�������Ă����ꍇ�A�I���}�[�N�ƌ��Ȃ��B
    
    name_ = findbuf.lname ;
    attr_ = (unsigned)findbuf.attrib;
    size_ = (filesize_t)(findbuf.hsize << 32) | findbuf.lsize;
    stamp_conv( findbuf.mdate,  findbuf.mtime , stamp_ );
    
    return 0;
}
#endif

/* Short FN �����g���Ȃ��ꍇ�̃t�@�C���o�b�t�@:�Œ� */
#if defined(OS2EMX)
    static _FILEFINDBUF4 findbuf;
    static ULONG findcount;
#elif defined(__DMC__)
    static struct find_t findbuf;
#elif defined(_MSC_VER)
	#include <windows.h>
	static WIN32_FIND_DATA wfd;
	static HANDLE hfind = INVALID_HANDLE_VALUE;
	static DWORD attributes;
#else
    static struct ffblk findbuf;
#endif

#ifdef _MSC_VER
static int findfirst( const char* path, DWORD attr)
{
    attributes = attr;
    hfind = ::FindFirstFile( path, &wfd);
    if( hfind==INVALID_HANDLE_VALUE){
	return -1;
    }
    return 0;
}
static int findnext()
{
    if( ::FindNextFile( hfind, &wfd)==FALSE){
	return -1;
    }
    return 0;
}
static int findclose()
{
    return ::FindClose( hfind );
}
#endif

/* ������ _dos_findfirst
 *	p_path - �p�X (���C���h�J�[�h���܂�)
 *	attr   - �A�g���r���[�g
 * return
 *	0 ... ����
 *	1 ... ���s(�Ō�̃t�@�C����������)
 *	-1 .. ���s(LFN���T�|�[�g����Ă��Ȃ�)
 */
unsigned NnDir::findfirst(  const char *path , unsigned attr )
{
    NnString path2(path);
    return NnDir::findfirst( path2 , attr );
}

unsigned NnDir::findfirst(  const NnString &p_path , unsigned attr )
{
    unsigned result;
    NnString path;
    filter( p_path.chars() , path );

#ifdef NYADOS
    union REGS in,out;
#ifdef USE_FAR_PTR
    struct SREGS segs;
#endif
    if( isLfnOk() ){
	in.h.cl = attr;
#ifdef USE_FAR_PTR
        segs.ds = FP_SEG(path.chars());
#endif
	in.x.dx = FP_OFF(path.chars());
	in.h.ch = 0 ;
	in.x.ax = 0x714E;
	in.x.si = DOS_DATE_FORMAT;

#ifdef USE_FAR_PTR
	if( (result = findcore(in,out ,segs))==0 ){
#else
	if( (result = findcore(in,out))==0 ){
#endif
	    handle = out.x.ax;
	    hasHandle = 1;
	}else{
	    hasHandle = 0;
	}
	return result;
    }
#endif
    hasHandle = 0;
    handle = 0;
#if defined(OS2EMX)
    /***  emx/gcc code for NYAOS-II ***/
    handle = 0xFFFFFFFF;
    findcount = 1 ;
    result = DosFindFirst(    (PUCHAR)path.chars() 
			    , (PULONG)&handle 
			    , attr 
			    , &findbuf
			    , sizeof(_FILEFINDBUF4) 
			    , &findcount 
			    , (ULONG)FIL_QUERYEASIZE );
    if( result == 0 ){
        name_ = findbuf.achName ;
	attr_ = findbuf.attrFile ;
	size_ = findbuf.cbFile ;
	stamp_conv( *(unsigned short*)&findbuf.fdateLastWrite ,
		    *(unsigned short*)&findbuf.ftimeLastWrite , stamp_ );
	hasHandle = 1;
    }
#elif defined(__DMC__) 
#if defined(__OS2__)
    FIND *findbuf=::findfirst( path.chars() , attr );
    if( findbuf != NULL ){
        name_ = findbuf->name;
        attr_ = findbuf->attribute;
    }
#else
    /*** Digitalmars C++ for NYADOS ***/
    result = ::_dos_findfirst( path.chars() , attr , &findbuf );
    if( result == 0 ){
        name_ = findbuf.name ;
	attr_ = findbuf.attrib;
	stamp_conv( findbuf.wr_date , findbuf.wr_time , stamp_ );
    }
#endif
#elif defined(_MSC_VER)
    /**** VC++ code for NYACUS but not maintenanced by hayama ****/
    result = ::findfirst( path.chars(), attr);
    if( result==0){
	name_ = wfd.cFileName;
	attr_ = wfd.dwFileAttributes;
        size_ = (((filesize_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow) ;
        stamp_conv( &wfd.ftLastWriteTime , stamp_ );
	hasHandle = 1;
    }
#else /*** Borland-C++ code for NYACUS ***/
    result = ::findfirst( path.chars() , &findbuf , (int)attr );
    if( result == 0 ){
	name_ = findbuf.ff_name;
	attr_ = findbuf.ff_attrib;
	size_ = findbuf.ff_fsize;
	stamp_conv( findbuf.ff_fdate , findbuf.ff_ftime , stamp_ );

	hasHandle = 1;
    }
#endif
    return result;
}

unsigned NnDir::findnext()
{
#ifdef NYADOS
    if( isLfnOk() ){
	union REGS in,out;
#ifdef USE_FAR_PTR
        struct SREGS segs;
#endif
	in.x.ax = 0x714F;
	in.x.bx = handle;
	in.x.si = DOS_DATE_FORMAT;
#ifdef USE_FAR_PTR
	return findcore(in,out,segs);
#else
	return findcore(in,out);
#endif
    }
#endif
#if defined(OS2EMX)
    /*** emx/gcc code for NYAOS-II(OS/2) ***/
    int result=DosFindNext(handle,&findbuf,sizeof(findbuf),&findcount);
    if( result == 0 ){
	name_ = findbuf.achName;
	attr_ = findbuf.attrFile;
	size_ = findbuf.cbFile ;
	stamp_conv( *(unsigned short*)&findbuf.fdateLastWrite ,
		    *(unsigned short*)&findbuf.ftimeLastWrite , stamp_ );
    }
#elif defined(__DMC__)
#  if defined(__OS2__)
    /*** DigitalMars C++ for NYAOS-II ***/
    int result=-1;
    struct FIND *entry=::findnext();
    if( entry != NULL ){
        result = 0;
        name_ = entry->name;
        attr_ = entry->attribute;
    }
#else
    /*** DigitalMars C++ for NYADOS ***/
    int result=::_dos_findnext( &findbuf );
    if( result == 0 ){
	name_ = findbuf.name;
	attr_ = findbuf.attrib;
	stamp_conv( findbuf.wr_date , findbuf.wr_time , stamp_ );
    }
#endif
#elif defined(_MSC_VER)
    /*** Visual C++ ***/
    int result = ::findnext();
    if( result==0){
	name_ = wfd.cFileName;
	attr_ = wfd.dwFileAttributes;
        size_ = (((filesize_t)wfd.nFileSizeHigh << 32) | wfd.nFileSizeLow) ;
        stamp_conv( &wfd.ftLastWriteTime , stamp_ );
    }
#else /*** Borland-C++ for NYACUS ***/
    int result=::findnext( &findbuf );
    if( result == 0 ){
	name_ = findbuf.ff_name ;
	attr_ = findbuf.ff_attrib;
	size_ = findbuf.ff_fsize;
	stamp_conv( findbuf.ff_fdate , findbuf.ff_ftime , stamp_ );

    }
#endif
    return result;
}

void NnDir::findclose()
{
#ifdef NYADOS
    union	REGS	in, out;
#endif

    if( hasHandle ){
#if defined(NYADOS)
	in.x.ax = 0x71A1;
	in.x.bx = handle;
	intdos(&in, &out);
#elif defined(OS2EMX)
        DosFindClose( handle );
#elif defined(_MSC_VER)
	::findclose();
#elif !defined(__DMC__)
	::findclose( &findbuf );
#endif
	hasHandle = 0;
    }
}

/* �J�����g�f�B���N�g���𓾂�.(LFN�Ή��̏ꍇ�̂�)
 *	pwd - �J�����g�f�B���N�g��
 * return
 *	0 ... ����
 *	1 ... ���s
 */
int NnDir::getcwd( NnString &pwd )
{
#ifdef NYADOS
    if( ! isLfnOk() ){
#endif
	char buffer[256];
#ifdef OS2EMX
	if( ::_getcwd2(buffer,sizeof(buffer)) != NULL ){
#else
	if( ::getcwd(buffer,sizeof(buffer)) != NULL ){
#endif
	    pwd = buffer;
	}else{
            pwd.erase();
        }
	return 0;
#ifdef NYADOS
    }
    union  REGS	 in, out;
    static char	tmpcur[] = ".";
    char   tmpbuf[256];

    in.x.cx = 0x8002;
    in.x.ax = 0x7160;
    in.x.si = FP_OFF(tmpcur);
    in.x.di = FP_OFF(tmpbuf);

#ifdef USE_FAR_PTR
    struct SREGS segs;
    segs.ds = FP_SEG(tmpcur);
    segs.es = FP_SEG(tmpbuf);
    intdosx(&in,&out,&segs);
#else
    intdos(&in, &out );
#endif

    // �G���[�̏ꍇ�A���������A�I��.
    if( out.x.cflag ){
        pwd.erase();
	return 1;
    }
    pwd = tmpbuf;
    return 0;
#endif
}

/* �X���b�V�����o�b�N�X���b�V���֕ϊ�����
 * �d������ \ �� / ����ɂ���.
 * 	src - ��������
 *	dst - ���ʕ�����̓��ꕨ
 */
void NnDir::f2b( const char *sp , NnString &dst )
{
    int prevchar=0;
    while( *sp != '\0' ){
	if( isKanji(*sp & 255) ){
	    prevchar = *sp;
	    dst << *sp++;
	    if( *sp == 0 )
		break;
	    dst << *sp++;
	}else{
	    if( *sp == '/' || *sp == '\\' ){
		if( prevchar != '\\' )
		    dst << '\\';
		prevchar = '\\';
		++sp;
	    }else{
		prevchar = *sp;
		dst << *sp++;
	    }
	}
    }
    // dst = sp; dst.slash2yen();
}


/* �p�X���ϊ����s���B
 * �E�X���b�V�����o�b�N�X���b�V���֕ϊ�
 * �E�`���_�����ϐ�HOME�̓��e�֕ϊ�
 * 	src - ��������
 *	dst - ���ʕ�����̓��ꕨ
 */
void NnDir::filter( const char *sp , NnString &dst_ )
{
    NnString dst;

    dst.erase();
    const char *home;
    if( *sp == '~'  && (isalnum( *(sp+1) & 255) || *(sp+1)=='_') ){
	NnString name;
	++sp;
	do{
	    name << *sp++;
	}while( isalnum(*sp & 255) || *sp == '_' );
	NnString *value=(NnString*)specialFolder.get(name);
	if( value != NULL ){
	    dst << *value;
	}else{
	    dst << '~' << name;
	}
    }else if( *sp == '~'  && 
             ( (home=getEnv("HOME",NULL))        != NULL  ||
               (home=getEnv("USERPROFILE",NULL)) != NULL ) )
    {
	dst << home;
	++sp;
    }else if( sp[0]=='.' &&  sp[1]=='.' && sp[2]=='.' ){
	extractDots( sp , dst );
    }
    dst << sp;

    f2b( dst.chars() , dst_ );
    // dst.slash2yen();
}

/* LFN�Ή� chdir.
 *	argv - �f�B���N�g����
 * return
 *	 0 - ����
 *	-1 - ���s
 */
int NnDir::chdir( const char *argv )
{
    NnString newdir;
    filter( argv , newdir );
#ifdef NYADOS
    if( isLfnOk() ){
	union REGS in,out;
	if( isAlpha(newdir.at(0)) && newdir.at(1)==':' ){
	    in.h.ah = 0x0E;
	    in.h.dl = (newdir.at(0) & 0x1F ) - 1;
	    intdos( &in , &out );
	}
	in.x.ax = 0x713B;
	in.x.dx = FP_OFF( newdir.chars() );
#ifdef USE_FAR_PTR
        struct SREGS segs;
        segs.ds = FP_SEG( newdir.chars() );
	intdosx(&in,&out,&segs);
#else
	intdos(&in,&out);
#endif
	return out.x.cflag ? -1 : 0;
    }
#endif
    if( isAlpha(newdir.at(0)) && newdir.at(1)==':' ){
#ifdef OS2EMX
        _chdrive( newdir.at(0) );
#else
        setdisk( (newdir.at(0) & 0x1F)-1 );
#endif
    }
    if(    newdir.at( newdir.length()-1 ) == '\\'
        || newdir.at( newdir.length()-1 ) == '/' )
        newdir += '.';

    if( newdir.iendsWith(".lnk") ){
	char buffer[ FILENAME_MAX ];
	if( read_shortcut( newdir.chars() , buffer , sizeof(buffer) )==0 ){
	    newdir = buffer;
	}
    }
    return ::chdir( newdir.chars() );
}

/* �t�@�C���N���[�Y
 * (����LFN�Ή��Ƃ����킯�ł͂Ȃ����A�R���p�C���ˑ��R�[�h��
 *  �����邽�ߍ쐬)
 *      fd - �n���h��
 */
void NnDir::close( int fd )
{
#ifdef NYADOS
    union REGS in,out;

    in.h.ah = 0x3e;
    in.x.bx = fd;
    intdos(&in,&out);
#else
    ::close(fd);
#endif
}
/* �t�@�C���o��
 * (����LFN�Ή��Ƃ����킯�ł͂Ȃ����A�R���p�C���ˑ��R�[�h��
 *  �����邽�ߍ쐬)
 *      fd - �n���h��
 *      ptr - �o�b�t�@�̃A�h���X
 *      size - �o�b�t�@�̃T�C�Y
 * return
 *      ���o�̓T�C�Y (-1 : �G���[)
 */
int NnDir::write( int fd , const void *ptr , size_t size )
{
#ifdef NYADOS
    union REGS in,out;
    struct SREGS sregs;

    in.h.ah  = 0x40;
    in.x.bx  = fd;
    in.x.cx  = size;
    in.x.dx  = FP_OFF(ptr);
    sregs.ds = FP_SEG(ptr);
    intdosx(&in,&out,&sregs);
    return out.x.cflag ? -1 : out.x.ax;
#else
    return ::write(fd,ptr,size);
#endif
}

#ifdef NYADOS
/* �p�X�����V���[�g�t�@�C�����֕ϊ�����B
 *     src - �����O�t�@�C����
 * return
 *     dst - �V���[�g�t�@�C����(static�̈�)
 */
const char *NnDir::long2short( const char *src )
{
    static char dst[ 67 ];
    
    dst[0] = '\0';

    union REGS in,out;
    struct SREGS sregs;

    in.x.ax = 0x7160;
    in.h.cl = 0x01;
    in.h.ch = 0x80;

    sregs.ds = FP_SEG(src);
    in.x.si  = FP_OFF(src);

    sregs.es = FP_SEG(dst);
    in.x.di  = FP_OFF(dst);
    intdosx(&in,&out,&sregs);
    if( out.x.cflag || dst[0] == '\0' )
	return src;
    
    return dst;
}


#endif

#ifdef NYADOS
/* �t�@�C���̏������݈ʒu�𖖔��ֈړ�������. */
int NnDir::seekEnd( int handle )
{
    union REGS in,out;

    // �A�y���h�̏ꍇ�A�t�@�C���̏������݈ʒu�𖖔��Ɉړ�������.
    in.x.ax = 0x4202;
    in.x.bx = handle;
    in.x.cx = 0;
    in.x.dx = 0;
    intdos(&in,&out);

    return out.x.cflag ? -1 : 0 ;
}
#endif

/* LFN �Ή� OPEN
 *      fname - �t�@�C����
 *      mode - "w","r","a" �̂����ꂩ
 * return
 *      �n���h��(�G���[��:-1)
 */
int NnDir::open( const char *fname , const char *mode )
{
#ifdef NYADOS
    if( mode == NULL )
        return -1;
    
    union REGS in,out;
    struct SREGS sregs;

    in.x.ax  = ( isLfnOk() ? 0x716C : 0x6C00 );

    // BX�̒l���ԈႦ��ƁA�I�[�v���͏o���邪,
    // �P�o�C�g���������߂Ȃ��̂Œ��ӂ��K�v.
    if( *mode == 'r' ){
        in.x.bx = 0x2000;
        in.x.dx = 0x01;
    }else{
        in.x.bx = 0x2001;                    // �A�N�Z�X/�V�F�A�����O���[�h.
        in.x.dx  = ( *mode == 'a' ? 0x11 : 0x12 );  // ����t���O.
    }
    in.x.cx  = 0x0000;                    // ����.
    in.x.si  = FP_OFF( fname );           // �t�@�C����.
    sregs.ds = FP_SEG( fname );           // �t�@�C����.
    intdosx( &in , &out , &sregs );

    if( out.x.cflag )
        return -1;

    int fd=out.x.ax;

    if( *mode == 'a' ){
	if( NnDir::seekEnd( fd ) != 0 ){
            NnDir::close(fd);
            return -1;
        }
    }
    return fd;
#else
    switch( *mode ){
    case 'w':
        return ::open(fname ,
                O_WRONLY|O_BINARY|O_CREAT|O_TRUNC , S_IWRITE|S_IREAD );
    case 'a':
        {
            int fd= ::open(fname ,
                O_WRONLY|O_BINARY|O_CREAT|O_APPEND , S_IWRITE|S_IREAD );
            lseek(fd,0,SEEK_END);
            return fd;
        }
    case 'r':
        return ::open(fname , O_RDONLY|O_BINARY , S_IWRITE|S_IREAD );
    default:
        return -1;
    }
#endif
}

/* �e���|�����t�@�C���������.  */
const NnString &NnDir::tempfn()
{
    static NnString result;

    /* �t�@�C��������(��f�B���N�g������)����� */
    char tempname[ FILENAME_MAX ];
    tmpnam( tempname );
    
    /* �f�B���N�g���������쐬 */
    const char *tmpdir;
    if(   ((tmpdir=getEnv("TEMP",NULL)) != NULL )
       || ((tmpdir=getEnv("TMP",NULL))  != NULL ) ){
        result  = tmpdir;
        result += '\\';
        result += tempname;
    }else{
        result  = tempname;
    }
    return result;
}

int NnDir::access( const char *path )
{
    NnDir dir(path); return *dir == NULL;
}

/* �J�����g�h���C�u��ύX����
 *    driveletter - �h���C�u����('A'..'Z')
 * return 0:����,!0:���s
 */
int  NnDir::chdrive( int driveletter )
{
    return 
#ifdef OS2EMX
	_chdrive( driveletter );
#else
	setdisk( (driveletter & 0x1F )- 1 );
#endif
}

/* �J�����g�h���C�u���擾����B
 * return �h���C�u���� 'A'...'Z'
 */
int NnDir::getcwdrive()
{
    return 
#ifdef OS2EMX
	_getdrive();
#elif defined(__TURBOC__)
	'A'+getdisk();
#else
	'A'+ _getdrive() - 1;
#endif
}

int NnFileStat::compare( const NnSortable &another ) const
{
    return name_.icompare( ((NnFileStat&)another).name_ );
}
NnFileStat::~NnFileStat(){}

#ifdef NYACUS
/* My Document �Ȃǂ� ~desktop �ȂǂƓo�^����
 * �Q�l��http://www001.upp.so-net.ne.jp/yamashita/doc/shellfolder.htm
 */
void NnDir::set_default_special_folder()
{
    static struct list_s {
	const char *name ;
	int  key;
    } list[]={
	{ "desktop"           , CSIDL_DESKTOPDIRECTORY },
	{ "sendto"            , CSIDL_SENDTO },
	{ "startmenu"         , CSIDL_STARTMENU },
	{ "startup"           , CSIDL_STARTUP },
	{ "mydocuments"       , CSIDL_PERSONAL },
	{ "favorites"         , CSIDL_FAVORITES },
	{ "programs"          , CSIDL_PROGRAMS },
	{ "program_files"     , CSIDL_PROGRAM_FILES },
	{ "appdata"           , CSIDL_APPDATA },
	{ "allusersdesktop"   , CSIDL_COMMON_DESKTOPDIRECTORY },
	{ "allusersprograms"  , CSIDL_COMMON_PROGRAMS },
	{ "allusersstartmenu" , CSIDL_COMMON_STARTMENU },
	{ "allusersstartup"   , CSIDL_COMMON_STARTUP },
	{ NULL , 0 },
    };
    LPITEMIDLIST pidl;
    char path[ FILENAME_MAX ];

    for( struct list_s *p=list ; p->name != NULL ; p++ ){
	path[0] = '\0';

	::SHGetSpecialFolderLocation( NULL , p->key, &pidl );
	::SHGetPathFromIDList( pidl, path );

	if( path[0] != '\0' )
	    specialFolder.put( p->name , new NnString( path ) );
    }
}

#else
void NnDir::set_default_special_folder()
{
    /* do nothing */
}
#endif

NnHash NnDir::specialFolder;

NnFileStat *NnFileStat::stat(const NnString &name)
{
    NnTimeStamp stamp1;
#ifdef _MSC_VER
    struct _stati64 stat1;
#else
    struct stat stat1;
#endif
    unsigned attr=0 ;
#ifdef __DMC__
    NnString name_( NnDir::long2short(name.chars()) );
#else
    NnString name_(name);
#endif
    if( name_.endsWith(":") || name_.endsWith("\\") || name_.endsWith("/") )
	name_ << ".";

#ifdef _MSC_VER
    if( ::_stati64( name_.chars() , &stat1 ) != 0 )
#else
    if( ::stat( name_.chars() , &stat1 ) != 0 )
#endif
    {
	return NULL;
    }
    if( stat1.st_mode & S_IFDIR )
	attr |= ATTR_DIRECTORY ;
#ifdef __DMC__
    if( (stat1.st_mode & S_IWRITE) == 0 )
#else
    if( (stat1.st_mode & S_IWUSR) == 0 )
#endif
	attr |= ATTR_READONLY ;
    
    stamp_conv( stat1.st_mtime , stamp1 );

    return new NnFileStat(
	name ,
	attr ,
	stat1.st_size ,
	stamp1 );
}
