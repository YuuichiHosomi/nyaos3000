#ifndef CONFIG_H
#define CONFIG_H

#if defined(__TURBOC__)  /* Borland���R���p�C�� */
#  if defined(__BORLANDC__)  /* Borland C++ */
#    define NYACUS                      /* NYACUS ���쐬���� */
#    undef  ESCAPE_SEQUENCE_OK          /* �G�X�P�[�v�V�[�P���X�����ߕs�\ */
#    undef  ARRAY_DELETE_NEED_SIZE      /* �z���delete�ɗv�f���͕s�v */
#    define TEMPLATE_OK
#  else                      /* Turbo C++ */
#    define NYADOS                      /* NYADOS ���쐬���� */
#    define ESCAPE_SEQUENCE_OK          /* �G�X�P�[�v�V�[�P���X�����߉\ */
#    define ARRAY_DELETE_NEED_SIZE      /* �z���delete�ɗv�f�����K�v */
#    define TEMPLATE_NG
#  endif
#elif defined(__DMC__)  /* DigitalMars C++ */
#   if defined(_MSDOS)
#     define  NYADOS
#     define  ESCAPE_SEQUENCE_OK        /* �G�X�P�[�v�V�[�P���X�����߉\ */
#     define  ASM_OK
#   elif defined(__OS2__)
#     define  OS2DMX
#     define  NYAOS2
#     define  ESCAPE_SEQUENCE_OK        /* �G�X�P�[�v�V�[�P���X�����߉\ */
#   else
#     define  NYACUS
#     undef   ESCAPE_SEQUENCE_OK        /* �G�X�P�[�v�V�[�P���X�����ߕs�\ */
#   endif
#   undef   ARRAY_DELETE_NEED_SIZE      /* �z���delete�ɗv�f���͕s�v */
#   define  TEMPLATE_OK
#elif defined(_MSC_VER)					/* VC */
#    define NYACUS                      /* NYACUS ���쐬���� */
#    undef  ESCAPE_SEQUENCE_OK          /* �G�X�P�[�v�V�[�P���X�����ߕs�\ */
#    undef  ARRAY_DELETE_NEED_SIZE      /* �z���delete�ɗv�f���͕s�v */
#    define TEMPLATE_OK
      /* _�֐��̒u������ */
#    include <direct.h>
#	define popen	_popen
#	define pclose	_pclose
#	define chdir	_chdir
#	define setdisk(d)	_chdrive(d+1)
#else                   /* emx/gcc */
#   define  NYAOS2
#   define  ESCAPE_SEQUENCE_OK          /* �G�X�P�[�v�V�[�P���X�����߉\ */
#   undef   ARRAY_DELETE_NEED_SIZE      /* �z���delete�ɗv�f���͕s�v */
#   define TEMPLATE_NG  /* �{���̓e���v���[�g Ok �����A�e�X�g���ł��Ȃ��̂� */
#endif

#if defined(__LARGE__) || defined(__COMPACT__)   /* TC++,DMC++���p */
#  define   USE_FAR_PTR 1                /* FAR�|�C���^�[���g�� */
#endif

#define COMPACT_LEVEL 0

#if defined(NYADOS)
#  define SHELL_NAME		"NYADOS"
#elif defined(NYACUS)
#  define SHELL_NAME		"NYACUS"
#elif defined(NYAOS2)
#  define SHELL_NAME		"NYAOS2"
#else
#  error NO SUPPORT COMPILER
#endif

#define RUN_COMMANDS "_nya"


#endif
