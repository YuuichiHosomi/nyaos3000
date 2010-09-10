#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "nndir.h"
#include "nnstring.h"
#include "shell.h"

/* �g���q�����t�@�C�����ł���� 1 ��Ԃ� */
static int has_dot(const char *path)
{
    int lastdot=NnString::findLastOf(path,"/\\.");
    if( lastdot >= 0 && path[ lastdot ] == '.' )
        return 1;
    else
        return 0;
}

/* �t���p�X�����O�ɂȂ��Ă���t�@�C�����ɁA.exe �g���q�����āA
 * ���݂����邩���m�F����
 *    nm - �t���p�X�����O�ɂȂ��Ă���t�@�C����
 *    has_dot_f - nm �̒��Ɂu.�v���܂܂�Ă�����^���Z�b�g����
 *    which - ���t���������ɁA������p�X�����i�[�����
 * return
 *     0 - ���t������(which �ɒl������)
 *    -1 - ���t����Ȃ�����
 *    -2 - ���t���������A���s�g���q�������Ȃ�
 */
static int exists( const char *nm , int has_dot_f , NnString &which )
{
    static const char *suffix_list[]={
        ".exe" , ".com" , ".bat" , ".cmd" , NULL 
    };
    NnString path(nm);

    for( const char **p=suffix_list ; *p != NULL ; ++p ){
        path << *p;
        if( NnDir::access(path.chars()) == 0 ){
            which = path ;
            return 0;
        }
        path.chop( path.length()-4 );
    }
    /* �g���q�̖����t�@�C���́A���Ƃ����݂��Ă����s�ł��Ȃ� */
    if( has_dot_f && NnDir::access(path.chars()) == 0 ){
        which = path ;
        return 0;
    }
    return -1;
}


/* ���s�t�@�C���̃p�X��T��
 *      nm      < ���s�t�@�C���̖��O
 *      which   > ���������ꏊ
 * return
 *      0 - ����
 *      -1 - �����炸
 */
int which( const char *nm, NnString &which )
{
    int has_dot_f = has_dot(nm);
    if( exists(nm,has_dot_f,which)==0 )
        return 0;

    /* ���΃p�X�w��E��΃p�X�w�肵�Ă�����̂́A
     * ���ϐ�PATH�����ǂ��Ă܂ŁA�������Ȃ�
     */
    if( NnString::findLastOf(nm,"/\\") >= 0 ){
        return -1;
    }

    NnString rest(".");
    const char *env=getEnv("PATH",NULL);
    if( env != NULL )
	rest << ";" << env ;
    
    while( ! rest.empty() ){
	NnString path;
	rest.splitTo( path , rest , ";" );
	if( path.empty() )
	    continue;
        path << '\\' << nm;
        if( exists(path.chars(),has_dot_f,which)==0 )
            return 0;
    }
    return -1;
}
