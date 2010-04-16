#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include "nndir.h"
#include "nnstring.h"
#include "shell.h"

/* �t���p�X�����O�ɂȂ��Ă���t�@�C�����ɁA.exe �g���q�����āA
 * ���݂����邩���m�F����
 *    nm - �t���p�X�����O�ɂȂ��Ă���t�@�C����
 *    which - ���t���������ɁA������p�X�����i�[�����
 * return
 *     0 - ���t������(which �ɒl������)
 *    -1 - ���t����Ȃ�����
 */
static int exists( const char *nm , NnString &which )
{
    NnString path(nm);
    if( NnDir::access(path.chars()) == 0 ){
        which = path ;
        return 0;
    }
    path << ".exe";
    if( NnDir::access(path.chars()) == 0 ){
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
    if( exists(nm,which)==0 )
        return 0;

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
        if( NnDir::access(path.chars())==0 ){
            which = path;
            return 0;
        }
        if( exists(path.chars(),which)==0 )
            return 0;
    }
    return -1;
}
