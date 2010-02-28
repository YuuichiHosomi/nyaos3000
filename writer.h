#ifndef WRITER_H
#define WRITER_H
#include "nnstring.h"
#include <stdio.h>

class Writer : public NnObject {
public:
    virtual ~Writer();
    virtual Writer &write( char c )=0;
    virtual Writer &write( const char *s)=0;
    virtual int isatty() const;

    Writer &operator << ( char c ){ return write(c); }
    Writer &operator << ( const char *s ){ return write(s); }
    Writer &operator << ( int n );
    Writer &operator << (const NnString &s){ return *this << s.chars(); }
};

/* <stdio.h> の FILE* を通じて出力する Writer系クラス.
 * ファイルのオープンクローズなどはしない。
 * ほとんど、stdout,stderr専用
 */
class StreamWriter : public Writer {
    FILE *fp_;
protected:
    FILE *fp(){ return fp_; }
    void setFp( FILE *fp ){ fp_ = fp; }
public:
    ~StreamWriter(){}
    StreamWriter() : fp_(NULL) {}
    StreamWriter( FILE *fp ) : fp_(fp){}
    Writer &write( char c );
    Writer &write( const char *s );
    int ok() const { return fp_ != NULL ; }
    int fd() const { return fileno(fp_); }
    int isatty() const;
};

/* 低水準I/Oインターフェイスを使って、画面出力をする。
 * - ファイルディスクリプタは自動クローズしない
 */
class RawWriter : public Writer {
    int fd_;
public:
    ~RawWriter();
    RawWriter(int fd=-1) : fd_(fd){}
    Writer &write( char c );
    Writer &write( const char *s );
    int ok() const { return fd_ >= 0 ; }
    int fd() const { return fd_; }
    void setFd(int fd){ fd_ = fd; }
    int isatty() const;
};

class FileWriter : public StreamWriter {
public:
    ~FileWriter();
    FileWriter( const char *fn , const char *mode );
};

class PipeWriter : public RawWriter {
    NnString cmds;
    void open( const NnString &cmds );
    int pid;
public:
    ~PipeWriter();
    PipeWriter( const char *cmds );
    PipeWriter( const NnString &cmds ){ pid=0;open(cmds); }
};

#ifdef NYACUS

/* エスケープシーケンスを解釈して、画面のアトリビュートの
 * コントロールまで行う出力クラス.
 */
class AnsiConsoleWriter : public RawWriter {
    static int default_color;
    int flag;
    int param[20];
    size_t n;
    enum { PRINTING , STACKING } mode ;
    enum { BAREN = 1 , GREATER = 2 } ;
public:
    AnsiConsoleWriter( int fd ) 
	: RawWriter(fd) , flag(0) , n(0) , mode(PRINTING) {}
    AnsiConsoleWriter( FILE *fp )
	: RawWriter(fileno(fp)) , flag(0) , n(0) , mode(PRINTING) {fflush(fp);}

    Writer &write( char c );
    Writer &write( const char *p );
};

#else

#define AnsiConsoleWriter StreamWriter

#endif

extern AnsiConsoleWriter conOut,conErr;

/* Writer のポインタ変数に対してリダイレクトする Writer クラス. */
class WriterClone : public Writer {
    Writer **rep;
public:
    WriterClone( Writer **rep_ ) : rep(rep_) {}
    ~WriterClone(){}
    Writer &write( char c ){ return (*rep)->write( c ); }
    Writer &write( const char *s ){ return (*rep)->write( s ); }
};

/* 出力内容を全て捨ててしまう Writer */
class NullWriter : public Writer {
public:
    NullWriter(){}
    ~NullWriter(){}
    Writer &write( char c ){ return *this; }
    Writer &write( const char *s ){ return *this; }
};



/* 標準出力・入力をリダイレクトしたり、元に戻したりするクラス */
class Redirect {
    int original_fd;
    int fd_;
    int isOpenFlag;
public:
    Redirect(int x) : original_fd(-1) , fd_(x) {}
    ~Redirect(){ reset(); }
    void close();
    void reset();
    void set(int x);
    int  fd() const { return fd_; }

    int switchTo( const NnString &fn , const char *mode );
};


#endif
