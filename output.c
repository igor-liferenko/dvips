/*
 *   These routines do most of the communicating with the printer.
 *
 *   LINELENGTH tells the maximum line length to send out.  It's been
 *   reduced to 72 because sometimes PostScript files are included in
 *   mail messages and many mailers mutilate longer lines.
 */
#define LINELENGTH (72)
#include "dvips.h" /* The copyright notice in that file is included too! */
#include <ctype.h>

#ifdef OS2
#include <stdlib.h>
#ifdef _MSC_VER
#define popen(pcmd, pmode)  _popen(pcmd, pmode)
#define pclose(pstream) _pclose(pstream)
#endif
#endif
/*
 *   The external routines called here:
 */
extern void error() ;
extern void send_headers() ;
extern int add_header() ;
extern FILE *search() ;
extern char *getenv() ;
#ifdef HPS
extern void fonttableout() ;
#endif
extern void makepsname() ;
extern void handlepapersize() ;
extern void findbb() ;
/*
 *   These are the external variables used by these routines.
 */
extern integer hh, vv ;
extern fontdesctype *curfnt ;
extern FILE *bitfile ;
extern char *oname ;
extern Boolean secure ;
extern Boolean reverse ;
extern Boolean removecomments ;
extern Boolean sendcontrolD, disablecomments, multiplesects ;
extern Boolean usesPSfonts, headers_off ;
extern Boolean safetyenclose ;
extern Boolean cropmarks ;
extern Boolean tryepsf ;
extern Boolean compressed ;
extern Boolean sepfiles ;
extern int numcopies ;
extern int collatedcopies ;
extern integer pagecopies ;
extern int totalpages ;
extern integer pagenum ;
extern Boolean manualfeed ;
extern int landscape ;
extern int quiet ;
extern int prettycolumn ;
extern int actualdpi, vactualdpi ;
extern char *iname, *nextstring ;
extern char *paperfmt ;
extern char *headerpath ;
extern char errbuf[] ;
extern shalfword linepos ;
extern char *figpath ;
extern struct header_list *ps_fonts_used ;
extern char banner[] ;
extern int gargc ;
extern char **gargv ;
extern struct papsiz *papsizes ;
extern integer hpapersize, vpapersize ;
char preamblecomment[256] ; /* usually "TeX output ..." */
void open_output() ;
#ifdef HPS
extern Boolean noprocset, HPS_FLAG ;
extern void set_bitfile() ;
#endif
/*
 *   We need a few statics to take care of things.
 */
static integer rhh, rvv ;
static int instring ;
static Boolean lastspecial = 1 ;
static shalfword d ;
static Boolean popened = 0 ;
int lastfont ; /* exported to dospecial to fix rotate.tex problem */
static void chrcmd();                   /* just a forward declaration */
static char strbuffer[LINELENGTH + 20], *strbp = strbuffer ;
static struct papsiz *finpapsiz ;
static struct papsiz defpapsiz = {
   0, 40258437L, 52099154L, "letter", ""
} ;
#ifdef CREATIONDATE
#if (!defined(VMS) && !defined(MSDOS) && !(defined(OS2) && defined(_MSC_VER)) && !defined(ATARIST))
 /* VAXC/MSDOS don't like/need this !! */
#include <sys/types.h>
#include <sys/time.h> /* time(), at least on BSD Unix */
#endif
#include <time.h> /* asctime() and localtime(), at least on BSD Unix */
static time_t jobtime;
#endif
/*
 *   This routine copies a file down the pipe.  Search path uses the
 *   header path.
 *
 *   We add code to handle the case of MS-DOS font files.
 *
 *   Format:  80 {01,02} four byte length in littleendian order data
 *   repeated possibly multiple times.
 */
static char *hxdata = "0123456789ABCDEF" ;
static int infigure ;
extern char *infont ;
static char *begbinary = "\n%%BeginBinary:" ;
void
copyfile(s)
        char *s ;
{
   FILE *f = NULL ;
   int c, prevc = '\n' ;
   long len ;
   char *bbmatch = begbinary ;
   /* begin DOS EPS code */
   int doseps = 0;
   unsigned long dosepsbegin, dosepsend = 0;
   /* end DOS EPS code */
#ifdef VMCMS
   register char *lastdirsep ;
   register char *trunc_s ;
   trunc_s = s ;
#endif
#ifdef MVSXA
   register char *lastdirsep ;
   register char *trunc_s ;
   trunc_s = s ;
#endif

   switch (infigure) {
   case 1:
/*
 *   Look in headerpath too, just in case.  This allows common header
 *   or figure files to be installed in the .../ps directory.
 */
      f = search(figpath, s, READ) ;
      if (f == 0)
         f = search(headerpath, s, READ) ;
#ifdef VMCMS
      lastdirsep = strrchr(s, '/') ;
      if ( NULL != lastdirsep ) trunc_s = lastdirsep + 1 ;
      (void)sprintf(errbuf,
   "Couldn't find figure file %s with CMS name %s; continuing", s, trunc_s) ;
#else
#ifdef MVSXA
      lastdirsep = strrchr(s, '/') ;
      if ( NULL != lastdirsep ) trunc_s = lastdirsep + 1 ;
      (void)sprintf(errbuf,
    "Couldn't find figure file %s with MVS name %s; continuing", s, trunc_s) ;
#else
      (void)sprintf(errbuf, "Couldn't find figure file %s; continuing", s) ;
#endif
#endif
      break ;
   default:
      f = search(headerpath, s, READ) ;
      (void)sprintf(errbuf, "! Couldn't find header file %s", s) ;
      break ;
#ifndef VMCMS
#ifndef MVSXA
#ifndef VMS
#ifndef MSDOS
#ifndef ATARIST
#ifndef __THINK__
   case 2:
#ifdef SECURE
      (void)sprintf(errbuf, "<%s>: Tick filename execution disabled", s) ;
#else
#ifdef OS2
      if (_osmode == OS2_MODE) {
#endif
      if (secure == 0)
         f = popen(s, "r") ;
#ifdef OS2
      }
#endif
      (void)sprintf(errbuf, "Failure to execute %s; continuing", s) ;
#endif
      break;
#endif
#endif
#endif
#endif
#endif
#endif
   }
   if (f==NULL)
      error(errbuf) ;
   else {
      if (! quiet) {
         if (strlen(s) + prettycolumn > STDOUTSIZE) {
            fprintf(stderr, "\n") ;
            prettycolumn = 0 ;
         }
#ifdef VMCMS
         (void)fprintf(stderr, "<%s>", trunc_s) ;
#else
#ifdef MVSXA
         (void)fprintf(stderr, "<%s>", trunc_s) ;
#else
         (void)fprintf(stderr, "<%s>", s) ;
#endif
#endif
         (void)fflush(stderr) ;
         prettycolumn += 2 + strlen(s) ;
      }
      if (linepos != 0)
#ifdef HPS
        if (!HPS_FLAG && (strcmp(s, "head.tmp") != 0)) /* no initial newline */
#endif
         (void)putc('\n', bitfile) ;
/*
 *   Suggested by Andrew Trevorrow; don't ever BeginFont a file ending in .enc
 */
      if (infont && strstr(s,".enc"))
         infont = 0 ;
      if (! disablecomments)
         if (infigure)
            (void)fprintf(bitfile, "%%%%BeginDocument: %s\n", s) ;
         else if (infont)
            (void)fprintf(bitfile, "%%%%BeginFont: %s\n", infont) ;
#ifdef HPS
         else if (noprocset) {}
#endif
         else
            (void)fprintf(bitfile, "%%%%BeginProcSet: %s\n", s) ;
      c = getc(f) ;
      if (c == 0x80) {
#if defined MSDOS || defined OS2 || defined(ATARIST)
         (void)close_file(f) ;  /* close MSDOS font file */
         f = search(headerpath, s, READBIN) ;  /* reopen in BINARY mode */
         (void)sprintf(errbuf, "! Couldn't find header file %s", s) ;
         if (f==NULL)
           error(errbuf) ;
         c = getc(f);
#endif
         while (1) {
            c = getc(f) ;
            switch(c) {
case 1:
case 2:
               len = getc(f) ;
               len += getc(f) * 256L ;
               len += getc(f) * 65536L ;
               len += getc(f) * 256L * 65536 ;
               if (c == 1) {
                  while (len > 0) {
                     c = getc(f) ;
                     if (c == EOF) {
                        error("premature EOF in MS-DOS font file") ;
                        len = 0 ;
                     } else {
                        if (c == 13)
                           (void)putc('\n', bitfile) ;
                        else
                           (void)putc(c, bitfile) ;
                        len-- ;
                     }
                  }
               } else {
                  putc('\n', bitfile) ;
                  prevc = 0 ;
                  while (len > 0) {
                     c = getc(f) ;
                     if (c == EOF) {
                        error("premature EOF in MS-DOS font file") ;
                        len = 0 ;
                     } else {
                        (void)putc(hxdata[c >> 4], bitfile) ;
                        (void)putc(hxdata[c & 15], bitfile) ;
                        len-- ;
                        prevc += 2 ;
                        if (prevc >= 76) {
                           putc('\n', bitfile) ;
                           prevc = 0 ;
                        }
                     }
                  }
               }
               break ;
case 3:
               goto msdosdone ;
default:
               error("saw type other than 1, 2, or 3 in MS-DOS font file") ;
               break ;
            }
            c = getc(f) ;
            if (c == EOF)
               break ;
            if (c != 0x80) {
               error("saw non-MSDOS header in MSDOS font file") ;
               break ;
            }
         }
msdosdone:
         prevc = 0 ;
      } else {
/* begin DOS EPS code */
         if (c == 'E'+0x80) {
            if ((getc(f)=='P'+0x80) && (getc(f)=='S'+0x80)
	                            && (getc(f)=='F'+0x80)) {
#if defined MSDOS || defined OS2
               (void)close_file(f) ;  /* close DOS EPS file */
               f = search(headerpath, s, READBIN) ;
                                                   /* reopen in BINARY mode */
               (void)sprintf(errbuf, "! Couldn't find header file %s", s) ;
               if (f==NULL)
                 error(errbuf) ;
               fseek(f, 4L, 0);
#endif
               doseps = 1;
               dosepsbegin = getc(f) ;
               dosepsbegin += getc(f) * 256L ;
               dosepsbegin += getc(f) * 65536L ;
               dosepsbegin += getc(f) * 256L * 65536 ;
               dosepsend = getc(f) ;
               dosepsend += getc(f) * 256L ;
               dosepsend += getc(f) * 65536L ;
               dosepsend += getc(f) * 256L * 65536 ;
               dosepsend += dosepsbegin;
#if defined MSDOS || defined OS2
               (void)close_file(f) ;  /* close DOS EPS file */
               f = search(headerpath, s, READ) ;  /* reopen in TEXT mode */
               (void)sprintf(errbuf, "! Couldn't find header file %s", s) ;
               if (f==NULL)
                 error(errbuf) ;
#endif
               fseek(f, dosepsbegin, 0);
               c = getc(f);
            }
            else {
               rewind(f);
               c = getc(f);
            }
         }
/* end DOS EPS code */
         if (c != EOF) {
            while (1) {
               if (c == *bbmatch) {
                  bbmatch++ ;
                  if (*bbmatch == '\0') {
                     integer size = 0 ;

                     if (removecomments)
                        (void)fputs(begbinary, bitfile) ;
                     (void)putc(c, bitfile) ;
                     while (1) {
                        c = getc(f) ;
                        if (c == ' ')
                           (void)putc(c, bitfile) ;
                        else
                           break ;
                     }
                     while ('0' <= c && c <= '9') {
                        size = size * 10 + c - '0' ;
                        (void)putc(c, bitfile) ;
                        c = getc(f) ;
                     }
                     while (c != '\r' && c != '\n') {
                        if (c == EOF)
                           error("! bad BeginBinary line in epsf") ;
                        (void)putc(c, bitfile) ;
                        c = getc(f) ;
                     }
                     (void)putc(c, bitfile) ;
                     for (; size>0; size--) {
                        c = getc(f) ;
                        if (c == EOF)
                           error("! premature end of file in binary section") ;
                        (void)putc(c, bitfile) ;
                     }
                     c = getc(f) ;
                     if (c == '\n' || c == '\r') {
                        (void)putc(c, bitfile) ;
                        c = getc(f) ;
                     }
                     if (c != '%')
                        error("! expected to see %%EndBinary at end of data") ;
                     while (1) {
                        (void)putc(c, bitfile) ;
                        if (c == '\r' || c == '\n')
                           break ;
                        c = getc(f) ;
                        if (c == EOF)
                           error("! premature end of file in binary section") ;
                     }
                     c = getc(f) ;
                  }
               } else
                  bbmatch = begbinary ;
               if (removecomments && c == '%' && prevc == '\n') {
                         /* skip comments */
		         /* revised:  only skip %% and %! comments */
                  c = getc(f) ;
                  if (c == '%' || c == '!') {
                     while ((c=getc(f))!=EOF) {
                        if (c == '\n' || c == '\r') {
                           c = '\n' ;
                           break ;
                        }
                     }
		  } else {
		     (void)putc('%', bitfile) ;
		     if (c != EOF)
                        (void)putc(c, bitfile) ;
		  }
#ifdef VMCMS
               } else if (c != 0x37 ) {
#else
#ifdef MVSXA
               } else if (c != 0x37 ) {
#else
               } else if (c != 4) {
#endif
#endif
                  (void)putc(c, bitfile) ;
               }
               prevc = c ;
/* begin DOS EPS code */
               if (doseps && (ftell(f)>=dosepsend))
                  break;      /* stop at end of DOS EPS PostScript section */
/* end DOS EPS code */
               c = getc(f) ;
               if (c == EOF)
                  break ;
               else if (c == '\r')
                  c = '\n' ;
            }
         }
      }
      if (prevc != '\n')
         (void)putc('\n', bitfile) ;
      linepos = 0 ;
#ifndef VMCMS
#ifndef MVSXA
#ifndef VMS
#ifndef MSDOS
#ifndef ATARIST
#ifndef __THINK__
      if (infigure == 2)
#ifdef OS2
         {
            if (_osmode == OS2_MODE)
               (void)pclose(f) ;
         }
#else
         (void)pclose(f) ;
#endif
      else
#endif
#endif
#endif
#endif
#endif
#endif
         (void)fclose(f) ;
      if (!disablecomments)
         if (infigure)
            (void)fprintf(bitfile, "\n%%%%EndDocument\n") ;
         else if (infont)
            (void)fprintf(bitfile, "\n%%%%EndFont\n") ;
#ifdef HPS
         else if (noprocset) {}
#endif
         else
            (void)fprintf(bitfile, "\n%%%%EndProcSet\n") ;
   }
}

/*
 *   For included PostScript graphics, we use the above routine, but
 *   with no fatal error message.
 */
void figcopyfile(s, systemtype)
char *s ;
int systemtype ;
{
   infigure = systemtype ? 2 : 1 ;
   copyfile(s) ;
   infigure = 0 ;
}
/*
 *   This next routine writes out a `special' character.  In this case,
 *   we simply put it out, since any special character terminates the
 *   preceding token.
 */
void
specialout(c)
        char c ;
{
   if (linepos >= LINELENGTH) {
      (void)putc('\n', bitfile) ;
      linepos = 0 ;
   }
   (void)putc(c, bitfile) ;
   linepos++ ;
   lastspecial = 1 ;
}

void
stringend()
{
   if (linepos + instring >= LINELENGTH - 2) {
      (void)putc('\n', bitfile) ;
      linepos = 0 ;
   }
   (void)putc('(', bitfile) ;
   *strbp = 0 ;
   (void)fputs(strbuffer, bitfile) ;
   (void)putc(')', bitfile) ;
   linepos += instring + 2 ;
   lastspecial = 1 ;
   instring = 0 ;
   strbp = strbuffer ;
}

void
scout(c)   /* string character out */
        char c ;
{
/*
 *   Is there room in the buffer?  LINELENGTH-6 is used because we
 *   need room for (, ), and a possible four-byte string \000, for
 *   instance.  If it is too long, we send out the string.
 */
   if (instring > LINELENGTH-6) {
      stringend() ;
      chrcmd('p') ;
   }
/*  changed next line to hex representation for VMCMS port
   if (c<' ' || c > 126 || c=='%' ) {
*/
   if ( c<0x20 || c>= 0x7F || c==0x25 ) {
      *strbp++ = '\\' ;
      *strbp++ = '0' + ((c >> 6) & 3) ;
      *strbp++ = '0' + ((c >> 3) & 7) ;
      *strbp++ = '0' + (c & 7) ;
      instring += 4 ;
   } else {
#ifdef VMCMS
     c = ascii2ebcdic[c];
#else
#ifdef MVSXA
     c = ascii2ebcdic[c];
#endif
#endif
     if (c == '(' || c == ')' || c == '\\') {
       *strbp++ = '\\' ;
       *strbp++ = c ;
       instring += 2 ;
     } else {
       *strbp++ = c ;
       instring++ ;
     }
   }
}

void
cmdout(s)
        char *s ;
{
   int l ;

   /* hack added by dorab */
   if (instring) {
        stringend();
        chrcmd('p');
   }
   l = strlen(s) ;
   if ((! lastspecial && linepos >= LINELENGTH - 20) ||
           linepos + l >= LINELENGTH) {
      (void)putc('\n', bitfile) ;
      linepos = 0 ;
      lastspecial = 1 ;
   } else if (! lastspecial) {
      (void)putc(' ', bitfile) ;
      linepos++ ;
   }
   (void)fputs(s, bitfile) ;
   linepos += l ;
   lastspecial = 0 ;
}


static void
chrcmd(c)
        char c ;
{
   if ((! lastspecial && linepos >= LINELENGTH - 20) ||
       linepos + 2 > LINELENGTH) {
      (void)putc('\n', bitfile) ;
      linepos = 0 ;
      lastspecial = 1 ;
   } else if (! lastspecial) {
      (void)putc(' ', bitfile) ;
      linepos++ ;
   }
   (void)putc(c, bitfile) ;
   linepos++ ;
   lastspecial = 0 ;
}

void
floatout(n)
float n ;
{
   char buf[20] ;

   (void)sprintf(buf, "%.2f", n) ;
   cmdout(buf) ;
}

void doubleout(n)
double n ;
{
   char buf[40] ;

   (void)sprintf(buf, "%g", n) ;
   cmdout(buf) ;
}

void
numout(n)
integer n ;
{
   char buf[10] ;

#ifdef SHORTINT
   (void)sprintf(buf, "%ld", n) ;
#else
   (void)sprintf(buf, "%d", n) ;
#endif
   cmdout(buf) ;
}

void
mhexout(p, len)
register unsigned char *p ;
register long len ;
{
   register char *hexchar = hxdata ;
   register int n, k ;

   while (len > 0) {
      if (linepos > LINELENGTH - 2) {
         (void)putc('\n', bitfile) ;
         linepos = 0 ;
      }
      k = (LINELENGTH - linepos) >> 1 ;
      if (k > len)
         k = len ;
      len -= k ;
      linepos += (k << 1) ;
      while (k--) {
         n = *p++ ;
         (void)putc(hexchar[n >> 4], bitfile) ;
         (void)putc(hexchar[n & 15], bitfile) ;
      }
   }
}

void
fontout(n)
        int n ;
{
   char buf[6] ;

   if (instring) {
      stringend() ;
      chrcmd('p') ;
   }
   makepsname(buf, n) ;
   cmdout(buf) ;
}

void
hvpos()
{
   if (rvv != vv) {
      if (instring) {
         stringend() ;
         numout(hh) ;
         numout(vv) ;
         chrcmd('y') ;
      } else if (rhh != hh) {
         numout(hh) ;
         numout(vv) ;
         chrcmd('a') ;
      } else { /* hard to get this case, but it's there when you need it! */
         numout(vv - rvv) ;
         chrcmd('x') ;
      }
      rvv = vv ;
   } else if (rhh != hh) {
      if (instring) {
         stringend() ;
         if (hh - rhh < 5 && rhh - hh < 5) {
#ifdef VMCMS /*  should replace 'p' in non-VMCMS line as well */
            chrcmd(ascii2ebcdic[(char)(112 + hh - rhh)]) ;
#else
#ifdef MVSXA /*  should replace 'p' in non-MVSXA line as well */
            chrcmd(ascii2ebcdic[(char)(112 + hh - rhh)]) ;
#else
            chrcmd((char)('p' + hh - rhh)) ;
#endif
#endif
         } else if (hh - rhh < d + 5 && rhh - hh < 5 - d) {
#ifdef VMCMS /* should replace 'g' in non-VMCMS line as well  */
            chrcmd(ascii2ebcdic[(char)(103 + hh - rhh - d)]) ;
#else
#ifdef MVSXA /* should replace 'g' in non-MVSXA line as well  */
            chrcmd(ascii2ebcdic[(char)(103 + hh - rhh - d)]) ;
#else
            chrcmd((char)('g' + hh - rhh - d)) ;
#endif
#endif
            d = hh - rhh ;
         } else {
            numout(hh - rhh) ;
            chrcmd('b') ;
            d = hh - rhh ;
         }
      } else {
         numout(hh - rhh) ;
         chrcmd('w') ;
      }
   }
   rhh = hh ;
}

/*
 *   initprinter opens the bitfile and writes the initialization sequence
 *   to it.
 */
void newline()
{
   if (linepos != 0) {
      (void)fprintf(bitfile, "\n") ;
      linepos = 0 ;
   }
   lastspecial = 1 ;
}

void
nlcmdout(s)
        char *s ;
{
   newline() ;
   cmdout(s) ;
   newline() ;
}
/*
 *   Is the dimension close enough for a match?  We use a quarter inch
 *   as a match; this is 65536*72.27/4 or 1,184,072 scaled points.
 */
static int indelta(i)
integer i ;
{
   if (i < 0)
      i = -i ;
   return (i <= 1184072) ;
}
/*
 *   A case-irrelevant string compare.
 */
int mlower(c)
int c ;
{
   if ('A' <= c && c <= 'Z')
      return c - 'A' + 'a' ;
   else
      return c ;
}
int ncstrcmp(a, b)
char *a, *b ;
{
   while (*a && (*a == *b ||
                       mlower(*a) == mlower(*b)))
      a++, b++ ;
   if (*a == 0 && *b == 0)
      return 0 ;
   else
      return 1 ;
}
/*
 *   Find the paper size.
 */
void findpapersize() {
   if (finpapsiz == 0) {
      struct papsiz *ps ;

      if (tryepsf && !landscape) {
         finpapsiz = &defpapsiz ;
         hpapersize = defpapsiz.xsize ;
         vpapersize = defpapsiz.ysize ;
         return ;
      }
      if (cropmarks) {
/*
 *   If user wanted crop marks, we increase the size of the page by
 *   a half inch all around.
 */
         if (hpapersize == 0 || vpapersize == 0) {
            error(
 "warning: -k crop marks wanted, but no paper size specified; using default") ;
            if (landscape) {
               hpapersize = defpapsiz.ysize ;
               vpapersize = defpapsiz.xsize ;
            } else {
               hpapersize = defpapsiz.xsize ;
               vpapersize = defpapsiz.ysize ;
            }
         }
         hpapersize += 2368143L ;
         vpapersize += 2368143L ;
         add_header(CROPHEADER) ;
      }
      if (paperfmt && *paperfmt) {
         for (ps = papsizes; ps; ps = ps->next)
            if (ncstrcmp(paperfmt, ps->name)==0)
               finpapsiz = ps ;
         if (finpapsiz == 0)
            error("no match for papersize") ;
      }
      if (finpapsiz == 0 && hpapersize > 0 && vpapersize > 0) {
         for (ps=papsizes; ps; ps = ps->next) {
            if (indelta(ps->xsize-hpapersize) &&
                indelta(ps->ysize-vpapersize)) {
               landscape = 0 ;
               break ;
            }
         }
         if (ps == 0) {
            for (ps=papsizes; ps; ps = ps->next) {
               if (indelta(ps->ysize-hpapersize) &&
                   indelta(ps->xsize-vpapersize)) {
                  landscape = 1 ;
                  break ;
               }
            }
            if (ps == 0) {
               for (ps=papsizes; ps; ps = ps->next) {
                  if (ps->ysize == 0 && ps->xsize == 0)
                     break ;
               }
               if (ps == 0) {
                  landscape = (hpapersize > vpapersize) ;
                  error(
                    "no match for special paper size found; using default") ;
               }
            }
         }
         finpapsiz = ps ;
      }
      if (finpapsiz == 0) {
         if (papsizes)
            finpapsiz = papsizes ;
         else
            finpapsiz = &defpapsiz ;
/*
 *   But change xsize/ysize to match so bounding box works.
 */
         if (hpapersize && vpapersize) {
            if (landscape) {
               finpapsiz->ysize = hpapersize ;
               finpapsiz->xsize = vpapersize ;
            } else {
               finpapsiz->xsize = hpapersize ;
               finpapsiz->ysize = vpapersize ;
            }
         }
      }
/*
 *   Here, there was no papersize special.  We set the paper size from
 *   the selected paper format.  If the selected paper format has no
 *   sizes, we use the defaults.
 */
      if (hpapersize == 0 || vpapersize == 0) {
         if (finpapsiz->xsize == 0 || finpapsiz->ysize == 0) {
            finpapsiz->xsize = defpapsiz.xsize ;
            finpapsiz->ysize = defpapsiz.ysize ;
         }
         if (landscape) {
            vpapersize = finpapsiz->xsize ;
            hpapersize = finpapsiz->ysize ;
         } else {
            hpapersize = finpapsiz->xsize ;
            vpapersize = finpapsiz->ysize ;
         }
/*
 *   Here, there was a papersize special, but the selected paper
 *   format has 0 0 for sizes.  We set the sizes here so that the
 *   bounding box works.
 */
      } else if (finpapsiz->xsize == 0 || finpapsiz->ysize == 0) {
         finpapsiz->xsize = hpapersize ;
         finpapsiz->ysize = vpapersize ;
/*
 *   Here, the user specified a size with -t, and there was a
 *   papersize special, and its sizes were greater than zero.
 *   We make sure the sizes are okay.  Note that the user must have
 *   specified landscape if this is desired.
 */
      } else if (paperfmt && *paperfmt) {
         if (landscape) {
            if (!indelta(vpapersize - finpapsiz->xsize) ||
                !indelta(hpapersize - finpapsiz->ysize)) {
               if (vpapersize > finpapsiz->xsize ||
                   hpapersize > finpapsiz->ysize)
                  error("warning: -t selected paper may be too small") ;
               else
                  error("note: -t selected paper may be too large") ;
            }
         } else {
            if (!indelta(hpapersize - finpapsiz->xsize) ||
                !indelta(vpapersize - finpapsiz->ysize)) {
               if (hpapersize > finpapsiz->xsize ||
                   vpapersize > finpapsiz->ysize)
                  error("warning: -t selected paper may be too small") ;
               else
                  error("note: -t selected paper may be too large") ;
            }
         }
      }
   }
}
/*
 *   Convert scaled points to PostScript points.  This is the same
 *   as return (i * 72 / (65536 * 72.27)), which is the same as
 *   dividing by 65781.76, but we want to round up.
 */
static int topoints(i)
integer i ;
{
   i += 65780L ;
   return (i / 6578176L)*100 + (i % 6578176) * 100 / 6578176 ;
}
/*
 *   Send out the special paper stuff.  If `hed' is non-zero, only
 *   send out lines starting with `!' else send all other lines out.
 */
void paperspec(s, hed)
char *s ;
int hed ;
{
   int sendit ;

   while (*s) {
      s++ ;
      if (*s == '\0')
         return ;
      if (*s == '!') {
         s++ ;
         while (*s == ' ') s++ ;
         sendit = hed ;
      } else
         sendit = ! hed ;
      if (sendit) {
         while (*s && *s != '\n')
            (void)putc(*s++, bitfile) ;
         putc('\n', bitfile) ;
      } else {
         while (*s && *s != '\n')
            s++ ;
      }
   }
}
char *epsftest(bop)
integer bop ;
{
   if (tryepsf && paperfmt == 0 && *iname) {
      findbb(bop+44) ;
      return nextstring ;
   }
   return 0 ;
}
static char *isepsf = 0 ;
static int endprologsent ;
void open_output() {
   if (*oname != 0) {
/*
 *   We check to see if the first character is a exclamation
 *   point, and popen if so.
 */
      if (*oname == '!' || *oname == '|') {
#ifdef MSDOS
            error("! can't open output pipe") ;
#else
#ifdef VMS
            error("! can't open output pipe") ;
#else
#ifdef VMCMS
            error("! can't open output pipe") ;
#else
#ifdef MVSXA
            error("! can't open output pipe") ;
#else
#ifdef __THINK__
            error("! can't open output pipe") ;
#else
#ifdef ATARIST
            error("! can't open output pipe") ;
#else
#ifdef OS2
         if (_osmode != OS2_MODE) {
            error("! can't open output pipe") ;
         } else {
#endif
         if (secure || (bitfile=popen(oname+1, "w"))==NULL)
            error("! couldn't open output pipe") ;
         else
            popened = 1 ;
#ifdef OS2
         }
#endif
#endif
#endif
#endif
#endif
#endif
#endif
      } else {
         if ((bitfile=fopen(oname,"w"))==NULL)
            error("! couldn't open PostScript file") ;
      }
   } else {
      bitfile = stdout ;
   }
}
void
initprinter(sect)
sectiontype *sect ;
{
   void tell_needed_fonts() ;
   int n = sect->numpages * pagecopies * collatedcopies ;
#ifdef HPS
   if (!HPS_FLAG)
#endif
      open_output() ;
   findpapersize() ;
   if (disablecomments)
      (void)fprintf(bitfile,
             "%%!PS (but not EPSF; comments have been disabled)\n") ;
   else {
      if (multiplesects)
         (void)fprintf(bitfile,
             "%%!PS (but not EPSF because of memory limits)\n") ;
      else {
         isepsf = epsftest(sect->bos) ;
         if (isepsf)
            (void)fprintf(bitfile, "%%!PS-Adobe-2.0 EPSF-2.0\n") ;
         else
            (void)fprintf(bitfile, "%%!PS-Adobe-2.0\n") ;
      }
      if (tryepsf && isepsf == 0)
         error("We tried, but couldn't make it EPSF.") ;
      (void)fprintf(bitfile, "%%%%Creator: %s", banner + 8) ;
      if (*iname)
         (void)fprintf(bitfile, "%%%%Title: %s\n", iname) ;
#ifdef CREATIONDATE
      jobtime=time(0);
      (void)fprintf(bitfile, "%%%%CreationDate: %s",
                                 asctime(localtime(&jobtime))) ;
#endif
      if (! isepsf) {
/*
 *   Normally, we wouldn't want to add that second field
 *   indicating that the page order is reversed, as per page
 *   644 of the Red book.  But we have to, for many existing
 *   spoolers.
 */
        (void)fprintf(bitfile, "%%%%Pages: %d%s\n", (sepfiles ? n : totalpages),
                                                    (reverse?" -1":"")) ;
        (void)fprintf(bitfile, "%%%%PageOrder: %sscend\n", reverse?"De":"A");
      }
      if (landscape) {
         fprintf(bitfile, "%%%%Orientation: Landscape\n") ;
         fprintf(bitfile, "%%%%BoundingBox: 0 0 %d %d\n",
              topoints(finpapsiz->xsize), topoints(finpapsiz->ysize)) ;
      } else if (isepsf)
         fprintf(bitfile, "%%%%BoundingBox: %s\n", isepsf) ;
      else
         fprintf(bitfile, "%%%%BoundingBox: 0 0 %d %d\n",
              topoints(finpapsiz->xsize), topoints(finpapsiz->ysize)) ;
      tell_needed_fonts() ;
      paperspec(finpapsiz->specdat, 1) ;
      (void)fprintf(bitfile, "%%%%EndComments\n") ;
   }
   {
      int i, len ;
      char *p ;

/*
 *   Here, too, we have to be careful not to exceed the line length
 *   limitation, if possible.
 */
      (void)fprintf(bitfile, "%%DVIPSCommandLine:") ;
      len = 18 ;
      for (i=0; i<gargc; i++) {
         p = gargv[i] ;
         while (*p > ' ')
            p++ ;
         if (*p)
            len += 2 ;
         len += strlen(gargv[i]) + 1 ;
         if (len > LINELENGTH) {
            (void)fprintf(bitfile, "\n%%+") ;
            len = strlen(gargv[i]) + 3 ;
            if (*p)
               len += 2 ;
         }
         (void)fprintf(bitfile, (*p ? " \"%s\"" : " %s"), gargv[i]) ;
      }
      (void)fprintf(bitfile, "\n%%DVIPSParameters: dpi=%d", actualdpi) ;
      if (actualdpi != vactualdpi)
         (void)fprintf(bitfile, "x%d", vactualdpi) ;
      if (compressed)
         (void)fprintf(bitfile, ", compressed") ;
      if (removecomments)
         (void)fprintf(bitfile, ", comments removed") ;
      (void)fputc('\n', bitfile) ;
   }
#ifdef VMCMS  /* convert preamblecomment to ebcdic so we can read it */
   {
      int i ;
      for ( i=0 ; preamblecomment[i] ; i++ )
          preamblecomment[i] = ascii2ebcdic[preamblecomment[i]] ;
   }
#else
#ifdef MVSXA   /* IBM: MVS/XA */
   {
      int i ;
      for ( i=0 ; preamblecomment[i] ; i++ )
          preamblecomment[i] = ascii2ebcdic[preamblecomment[i]] ;
   }
#endif  /* VMCMS */
#endif
   (void)fprintf(bitfile, "%%DVIPSSource: %s\n", preamblecomment) ;
   linepos = 0 ;
   endprologsent = 0 ;
   if (safetyenclose)
      (void)fprintf(bitfile, "/SafetyEnclosure save def\n") ;
   if (! headers_off)
      send_headers() ;
}
void setup() {
   newline() ;
   if (endprologsent == 0 && !disablecomments) {
      (void)fprintf(bitfile, "%%%%EndProlog\n") ;
      (void)fprintf(bitfile, "%%%%BeginSetup\n") ;
      if (vactualdpi == actualdpi)
         (void)fprintf(bitfile, "%%%%Feature: *Resolution %ddpi\n",
                                           actualdpi) ;
      else
         (void)fprintf(bitfile, "%%%%Feature: *Resolution %dx%ddpi\n",
                                           actualdpi, vactualdpi) ;
      if (multiplesects && *(finpapsiz->specdat)) {
         (void)fprintf(bitfile, "TeXDict begin\n") ;
         paperspec(finpapsiz->specdat, 0) ;
         (void)fprintf(bitfile, "end\n") ;
      }
      if (manualfeed)
         (void)fprintf(bitfile, "%%%%Feature: *ManualFeed True\n") ;
#ifdef HPS
      if (!HPS_FLAG)
#endif
      if (multiplesects)
         (void)fprintf(bitfile, "%%%%EndSetup\n") ;
   }
   if (multiplesects && ! disablecomments)
      (void)fprintf(bitfile, "%%DVIPSBeginSection\n") ;
   cmdout("TeXDict") ;
   cmdout("begin") ;
   if (endprologsent || disablecomments || multiplesects == 0) {
      (void)fprintf(bitfile, "\n") ;
      paperspec(finpapsiz->specdat, 0) ;
   }
   if (manualfeed) cmdout("@manualfeed") ;
   if (landscape) cmdout("@landscape") ;
   if (numcopies != 1) {
      numout((integer)numcopies) ;
      cmdout("@copies") ;
   }
   if (endprologsent == 0 && !disablecomments) {
      newline() ;
      endprologsent = 1 ;
#ifdef HPS
      if (!HPS_FLAG)
#endif
         if (! multiplesects)
            (void)fprintf(bitfile, "%%%%EndSetup\n") ;
   }
#ifdef HPS
  if (HPS_FLAG) {
    fclose(bitfile) ;
    set_bitfile("body.tmp",0) ;
  }
#endif
}
/*
 *   cleanprinter is the antithesis of the above routine.
 */
void
cleanprinter()
{
   (void)fprintf(bitfile, "\n") ;
   (void)fprintf(bitfile, "userdict /end-hook known{end-hook}if\n") ;
   if (safetyenclose)
      (void)fprintf(bitfile, "SafetyEnclosure restore\n") ;
   if (!disablecomments)
      (void)fprintf(bitfile, "%%%%EOF\n") ;
   if (sendcontrolD)
      (void)putc(4, bitfile) ;
   if (ferror(bitfile))
      error("Problems with file writing; probably disk full.") ;
#ifndef MSDOS
#ifndef VMS
#ifndef MVSXA
#ifndef VMCMS
#ifndef __THINK__
#ifndef ATARIST
#ifdef OS2
   if (_osmode == OS2_MODE)
#endif
      if (popened)
         (void)pclose(bitfile) ;
#endif
#endif
#endif
#endif
#endif
#endif
   if (popened == 0)
      (void)fclose(bitfile) ;
   bitfile = NULL ;
}

/* this tells dvips that it has no clue where it is. */
static int thispage = 0 ;
static integer rulex, ruley ;
void psflush() {
   rulex = ruley = rhh = rvv = -314159265 ;
   lastfont = -1 ;
}
/*
 *   pageinit initializes the output variables.
 */
void
pageinit()
{
   psflush() ;
   newline() ;
   thispage++ ;
   if (!disablecomments)
      if (multiplesects)
#ifdef SHORTINT
         (void)fprintf(bitfile, "%%DVIPSSectionPage: %ld\n", pagenum) ;
      else if (! isepsf)
         (void)fprintf(bitfile, "%%%%Page: %ld %d\n", pagenum, thispage) ;
#else
         (void)fprintf(bitfile, "%%DVIPSSectionPage: %d\n", pagenum) ;
      else if (! isepsf)
         (void)fprintf(bitfile, "%%%%Page: %d %d\n", pagenum, thispage) ;
#endif
   linepos = 0 ;
   numout((integer)pagenum) ;
   numout((integer)thispage-1) ;
   cmdout("bop") ;
   d = 0 ;
}



/*
 *   This routine ends a page.
 */
void
pageend()
{
   if (instring) {
      stringend() ;
      chrcmd('p') ;
   }
   cmdout("eop") ;
}

/*
 *   drawrule draws a rule at the specified position.
 *   It does nothing to save/restore the current position,
 *   or even draw the current string.  (Rules are normally
 *   set below the baseline anyway, so this saves us on
 *   output size almost always.)
 */
void
drawrule(rw, rh)
        integer rw, rh ;
{
   numout((integer)hh) ;
   numout((integer)vv) ;
   if (rw == rulex && rh == ruley)
      chrcmd('V') ;
   else {
      numout((integer)rw) ;
      numout((integer)rh) ;
      chrcmd('v') ;
      rulex = rw ;
      ruley = rh ;
   }
}

/*
 *   drawchar draws a character at the specified position.
 */
void
drawchar(c, cc)
        chardesctype *c ;
        int cc ;
{
   hvpos() ;
   if (lastfont != curfnt->psname) {
      fontout((int)curfnt->psname) ;
      lastfont = curfnt->psname ;
   }
   scout(cc) ;
   rhh = hh + c->pixelwidth ; /* rvv = rv */
}
/*
 *   This routine sends out the document fonts comment.
 */
void tell_needed_fonts() {
   struct header_list *hl = ps_fonts_used ;
   char *q ;
   int roomleft = -1 ;
   extern char *get_name() ;

   if (hl == 0)
      return ;
   while (0 != (q=get_name(&hl))) {
      if ((int)strlen(q) >= roomleft) {
         if (roomleft != -1) {
            fprintf(bitfile, "\n%%%%+") ;
            roomleft = LINELENGTH - 3 ;
         } else {
            fprintf(bitfile, "%%%%DocumentFonts:") ;
            roomleft = LINELENGTH - 16 ;
         }
      }
      fprintf(bitfile, " %s", q) ;
      roomleft -= strlen(q) + 1 ;
   }
   fprintf(bitfile, "\n") ;
}
