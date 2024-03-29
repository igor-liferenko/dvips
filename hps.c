/* This is the main file for hacking dvips to do HyperPostScript
 * Written by Mark D. Doyle 11/94. It is (C) Copyright 1994 by Mark D. Doyle
 * and the University of California. You may modify and use this program to
 * your heart's content, so long as you send modifications to Mark Doyle and
 * abide by the rest of the dvips copyrights. 
 */
#ifdef HPS
#include "dvips.h"
#include <stdlib.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef Tolower
#define Tolower tolower
#endif
#define NAME 0
#define HREF 1
#define skip_space(s) for( ; *s == ' ' ; s++) ;

Boolean inHTMLregion = 0 ;
Boolean NO_ERROR = 1 ;
Boolean HPS_ERROR = 0 ;
integer HREF_COUNT ;  
Boolean ISHREF = 0 ;
Boolean POPPED = 0 ;
Boolean PUSHED = 0 ;

void do_html() ; /* convert html special into hps link */
int href_or_name() ; /* Check for href or name in special */
int parseref() ; /* Look for = quotes. Return string between quotes */
int get_string() ; /* Return string between quotes */
int do_link() ; /* handle href in special */
unsigned hash_string() ;
char *current_name ;
int current_type ;
int current_pushcount ;
extern integer pushcount ;


#define GoTo 0
#define GoToR 1
#define Launch 2

typedef struct rectangle {
  double llx ; /* lower left x coor */
  double lly ; /* lower left y coor */
  double urx ; /* upper right x coor */
  double ury ; /* upper right y coor */
  } Rectangle ; 

typedef struct hps_link {
  int action ; /* GoTo, GoToR, or Launch */
  char *file ;   /* PDF-style system-independent filename - unused now */
  Rectangle rect ;  /* rectangle for location of link */
  int border[5] ; /* Border info --- with dashes*/
  int srcpg ; /* Page of location */
  int page ;  /* Page of destination --- left out is same as srcpg */
  int vert_dest ; /* Vertical position of destination on page */
  double color[3] ; /* color: length of array gives colorspace (3 <=> RGB)*/
  char *title ; /* Title of link */
  } Hps_link ;

struct nlist { /* hashtable entry */
  struct nlist *next ; /* next entry in chain */
  char *name ; /* defined name */
  Hps_link *defn ; /* Link associated with name */
  } ;

typedef struct rect_list { /* linked list of rectangles */
  struct rect_list *next ; /* next rectangle in the chain */
  Rectangle rect ;
  } Rect_list ;

Rect_list *current_rect_list ;

#define HASHSIZE 1223 
static struct nlist *link_targets[HASHSIZE] ; /* names .... */
static struct nlist *link_sources[HASHSIZE] ; /* hrefs .... */

char *dup_str() ; /* duplicate a string */
Hps_link *link_dup() ; /* duplicate a link struct */
struct nlist *lookup_link() ; /* lookup a link in one of the hashtables */
struct nlist *install_link() ; /* install a link in one of the hashtables */
void *rem_link() ; /* remove a link from a hashtable NOT USED */

Hps_link *target_link() ; /* Finds a target link given an href */
int href_name_match() ; /* Matches strings for hashtable lookup */
double dvi_to_hps_conv() ; /* Converts dvi coordinates to hps coor */
void stamp_hps() ; /* Print a pdfmark with link info into the file */
void stamp_external() ; /* For external links */
void set_bitfile() ; /* Redirect the output to a different file */
void finish_hps() ; /* Put together all of the files and output them out */
int count_targets() ; /* Count the number of targets that have been defined */
void do_target_dict() ; /* Produce PostScript dictionary for targets */
extern void copyfile() ;
extern void open_output() ;
extern Boolean removecomments ;
Boolean noprocset ; /* Leave out BeginProc and EndProc comments */
int vert_loc() ; /* Give a vertical position for the FitH param */
void vertical_in_hps() ; /* Handle broken line href's */
void print_rect_list() ; /* print out the list of rect's for broken line */
void end_current_box() ;
void start_new_box() ;

extern int vactualdpi ; 
extern int pagecounter ;
extern void cmdout() ;
extern integer hhmem, vvmem ;
/* Temporary hack to get hps coordinates right. Should be fixed. */
#define PAGESIZE 792
#define MARGIN 72
#define VERTICAL 0
#define HORIZONTAL 1
#define FUDGE 2.0

/* For later use 
* static char *dest_key[9] = {"Fit", "FitB", "FitW", "FitH", "FitBH"
               "FitR", "FitV", "FitBV", "XYZ"} ; 
*/

extern FILE *bitfile  ; 
extern char *oname  ; 

extern void error() ;
extern char Tolower() ; 

extern integer pagenum ;
extern integer hh ;
extern integer vv ;
extern integer hoff, voff ;

char *hs = NULL ; /* html string to be handled */
char *url_name = NULL ; /* url between double quotes */

/* parse anchor into link */
void do_html(s)
char *s ;
{
Hps_link *nl ;
url_name = (char *)malloc(strlen(s)+1) ;
hs = s ;
HREF_COUNT = 0 ; 
skip_space(hs) ; /* skip spaces */
if ( Tolower(*hs) == 'a') { /* valid start */
  POPPED = FALSE ;
  if (inHTMLregion) {
   error("previous html not closed with /A");
   return;
  } else {
     inHTMLregion = TRUE ;
     current_rect_list = NULL ;
     }
  hs++ ;
  skip_space(hs) ;
  while(*hs != '\0') {
/* parse for names and href */
  if (!href_or_name()) { 
    error("Bad HMTL:") ;
  error(s) ;
  error("!") ;
    break ;
    }
  skip_space(hs) ;
      }
    } else if ( (*hs == '/') && (Tolower(hs[1]) == 'a') ) {
              if (!inHTMLregion) {
                error("In html, /A found without previous A") ;
		return;
                } else {
            inHTMLregion = FALSE ;
                    }
       if (current_type == HREF && current_name[0] != '#') {
	  if ((nl = lookup_link(current_name, current_type)->defn)) { 
	    nl->rect.urx = dvi_to_hps_conv(hh + hoff,HORIZONTAL) ;
	    nl->rect.ury = dvi_to_hps_conv(vv + voff, VERTICAL)-FUDGE+12.0 ; 
	    stamp_external(current_name,nl); /* Broken lines ? */
	  } else {
	    error("!Null lookup");
	  }
       } else {
              if ((nl = lookup_link(current_name, current_type)->defn)) { 
              nl->rect.urx = dvi_to_hps_conv(hh + hoff,HORIZONTAL) ;
              nl->rect.ury = dvi_to_hps_conv(vv + voff, VERTICAL)-FUDGE+12.0 ; 
              if (current_type) {
                stamp_hps(nl) ; /* Put link info right where special is */
                print_rect_list() ; /* print out rectangles */
                }
                } else {
                    error("!Null lookup");
                    }
              } 
       }
              else {
                error( "No A in html special") ;
        error(s) ;
        /*error("!") ;*/
                }
      
return ;
}

int href_or_name() {
if ((strncmp(hs, "href", 4) == 0) || (strncmp(hs, "HREF", 4) == 0)) {
    ISHREF = TRUE ;
    } else if ((strncmp(hs, "name", 4) == 0) 
              || (strncmp(hs, "NAME", 4) == 0)) {
        ISHREF = FALSE ; 
        } else {
          error("Not valid href or name html reference") ;
          return(HPS_ERROR) ;
          }
  if(!parseref()) return(HPS_ERROR) ;
  if (url_name) {
    if( ISHREF ) {
      do_link(url_name, HREF) ;
    } else do_link(url_name, NAME) ;
  } else {
      error("No string in qoutes ") ;
      return(HPS_ERROR) ;
      }
  return(NO_ERROR) ;
}

int parseref() {
int i = 0 ;
  for(i=0 ; i++ < 4 ; hs++) ; /* skip href or name in html string */
  skip_space(hs) ;
  if (*hs == '=' ) {
    hs++ ;
    } else {
      error("No equal sign") ;
      return(HPS_ERROR) ;
      }
  if(!get_string()) return(HPS_ERROR) ;
  return(NO_ERROR) ; /* extract stuff between double quotes */
}
        
int get_string() {
char *v = url_name ; 

  skip_space(hs) ;
 /* hash_name() ; */
  if (*hs == '"') {
    for(hs++ ; *hs != '"' && *hs != '\0' ; *v++ = *hs++) ;  /* need to esc " */
    if(*hs == '"') {
      hs++ ;
    *v++ = '\0' ;
      return(NO_ERROR) ;
      } else {
        error("Improper href. Missing closing double quote") ;
        return(HPS_ERROR) ;
        }
    } else {
      error("Improper href. Missing opening double quote") ;
      return(HPS_ERROR) ;
    }
}

int do_link(s, type)
char *s ;
int type ;
{

Hps_link *p ;

if (HREF_COUNT++ > 0) {
  error("!HTML string contains more than one href") ;
  return(HPS_ERROR) ;
  }
  p = (Hps_link *)malloc(sizeof(*p)) ;
  p->action = GoTo ;
  p->title = (char *)malloc(strlen(s)+1) ;
  p->title = s ;
  p->srcpg = pagecounter ;
  p->rect.llx = dvi_to_hps_conv(hh + hoff,HORIZONTAL) ; 
  p->rect.lly = dvi_to_hps_conv(vv + voff,VERTICAL)-FUDGE ;
  p->rect.urx = -1.0 ;
  p->rect.ury = -1.0 ;
  p->vert_dest = -1 ; 
  p->page = -1 ;
  p->color[0] = 0 ;
  p->color[1] = 0 ; /* Blue links */
  p->color[2] = 1 ;
  p->border[0] = 1 ; /* horizontal corner radius */
  p->border[1] = 1 ; /* vertical corner radius */
  p->border[2] = 1 ; /* box line width */
  p->border[3] = 3 ; /* dash on size */
  p->border[4] = 3 ;  /* dash off size */

  current_name = (char *)malloc(strlen(s)+1) ; 
  current_name = s ; 
  current_type = type ;
  current_pushcount = pushcount ;
  install_link(s, p, type) ;
  return(NO_ERROR) ;
}
                 
unsigned int hash_string(s) 
char *s ;
{
  unsigned hashval ;
  for (hashval = 0 ; *s != '\0' ; s++)
    hashval = *s + 31 * hashval ;
  return hashval % HASHSIZE ;
}

/* lookup a hashed name */ 

struct nlist *lookup_link(s, type)
char *s ;
int type ;
{
  struct nlist *np ;
  
  for(np = type ? link_sources[hash_string(s)] : link_targets[hash_string(s)] ;
        np != NULL ; np = np -> next)
      if (strcmp(s, np->name) == 0)
      return np ; /* found */
  return NULL ; /* not found */
}

struct nlist *install_link(name, defn, type)
char *name ;
Hps_link *defn ;
int type ;
{
  struct nlist *np ;
  unsigned hashval ;
    np = (struct nlist *)malloc(sizeof(*np)) ;
    if (np == NULL || (np->name = dup_str(name)) == NULL)
      return NULL ;
    hashval = hash_string(name) ;
    np->next = type ? link_sources[hashval] : link_targets[hashval] ;
    (type ? link_sources : link_targets)[hashval] = np ;
    if ((np->defn = link_dup(defn)) == NULL)
      return NULL ; 
  return np ;
}

char *dup_str(w) /* make a duplicate of s */
char *w ;
{
  char *p ;
  p = (char *)malloc(strlen(w)+1) ;
  if (p != NULL)
    strcpy(p,w) ;
  return p ;
}

Hps_link *link_dup(s) /* make a duplicate link */
Hps_link *s ;
{
  Hps_link *p ;
  
  p = (Hps_link *) malloc(sizeof(*p)) ;
  if (p != NULL)
    p = s ;
  return p ;
}

double dvi_to_hps_conv(i, dir)
int i, dir ;
{
double hps_coor ;
/* Convert dvi integers into proper hps coordinates
   Take into account magnification and resolution that dvi file was
   produced at */ 
   hps_coor = dir ? (((i * 72.0) / vactualdpi) +MARGIN) : (PAGESIZE - ((i * 72.0) / (vactualdpi)) - MARGIN  ) ;
   return(hps_coor) ;
}

int vert_loc(i)
int i ;
{
  int return_value ;
  return_value = (i + (PAGESIZE / 4) + FUDGE) ; 
  if ( return_value > PAGESIZE) {
    return((int)PAGESIZE) ;
    } else if (return_value <  (PAGESIZE / 4.0)) {
        return((PAGESIZE / 4)) ;
        } else return(return_value) ;
}

Hps_link *dest_link(s)
char *s ;
{
/* Assume for now that only one entry with same NAME, i.e.
   Should be true for all legitimate papers.
   Also, assume prepending of # for names. */
  
  struct nlist *np ;
  
  s++ ; /* get rid of hashmark */
  for(np = link_targets[hash_string(s)] ; np != NULL ; np = np -> next) {
     if ( href_name_match(s, np->name)) {
      return (np->defn) ; /* found */
      }
      }
  error("Oh no, link not found in target hashtable") ;
 error(s) ;
 error("!") ;
  return NULL ; /* not found */
}

int count_targets() {
int count=0 ;
int i ;
struct nlist *np ;

for (i = 0 ; i < HASHSIZE ; i++) 
  for(np = link_targets[i]  ; np != NULL ; np = np -> next) 
      count++ ; 
  return count ;      
}

void do_targets() {

struct nlist *np ;
int i ;
Hps_link *dest ;

for (i = 0 ; i < HASHSIZE ; i++) 
  for(np = link_sources[i]  ; np != NULL ; np = np -> next) {
      if (np->name[0] == '#') { 
      dest = dest_link(np->name) ;
      np->defn->page = dest->srcpg ;
      np->defn->vert_dest = vert_loc((int) dest->rect.lly) ;
      } 
      }
}

void do_target_dict()
{
struct nlist *np ;
int i ;
(void) fprintf(bitfile, "HPSdict begin\n") ;
(void)fprintf(bitfile, "/TargetAnchors\n") ;
(void)fprintf(bitfile, "%i dict dup begin\n",count_targets()) ;

for (i = 0 ; i < HASHSIZE ; i++) 
  for(np = link_targets[i]  ; np != NULL ; np = np -> next) 
    (void)fprintf(bitfile, "(%s) [%i [%.0f %.0f %.0f %.0f] %i] def\n",
              np->defn->title, np->defn->srcpg, 
              np->defn->rect.llx, np->defn->rect.lly,
              np->defn->rect.urx, np->defn->rect.ury,
              vert_loc((int) np->defn->rect.lly)) ;  
  (void)fprintf(bitfile,"end targetdump-hook def\n") ;
}

int href_name_match(h, n)
char *h, *n ;
{
  int count = 0 ;
  int name_length = strlen(n) ;
  while((*h == *n) & (*h != '\0') )  {
    count++ ;
    h++ ;
    n++ ;
    }
  if (name_length == count) {
    return 1 ;
    } else {
     /* printf(" count: %i \n", count) ; */
    return 0 ;
    }
}

void stamp_hps(pl)
Hps_link *pl ;
{
char tmpbuf[200] ;
  if (pl == NULL) {
    error("Null pointer, oh no!") ;
    return ;
    } else {
      /* print out the proper pdfm with local page info only 
       *  target info will be in the target dictionary */
(void)sprintf(tmpbuf, 
" (%s) [[%.0f %.0f %.0f %.0f] [%i %i %i [%i %i]] [%.0f %.0f %.0f]] pdfm ", pl->title, pl->rect.llx, pl->rect.lly, pl->rect.urx, pl->rect.ury,
pl->border[0], pl->border[1], pl->border[2], pl->border[3],pl->border[4],
    pl->color[0], pl->color[1], pl->color[2]) ;
    cmdout(tmpbuf) ; 
    }

}

/* For external URL's, we just pass them through as a string. The hyperps
 * interpreter can then do what is wants with them.
 */
void stamp_external(s, pl) 
char *s ;
Hps_link *pl ;
{
  char tmpbuf[200];
  if (pl == NULL) {
    error("Null pointer, oh no!") ;
    return ;
  } else {
    /* print out the proper pdfm with local page info only 
     *  target info will be in the target dictionary */
    (void)sprintf(tmpbuf," [[%.0f %.0f %.0f %.0f] [%i %i %i [%i %i]] [%.0f %.0f %.0f]] (%s) pdfm ", pl->rect.llx, pl->rect.lly, pl->rect.urx, pl->rect.ury,
		  pl->border[0], pl->border[1], pl->border[2], pl->border[3],pl->border[4],
		  pl->color[0], pl->color[1], pl->color[2], s) ;
    cmdout(tmpbuf) ;
  }
}

void finish_hps() {
  fclose(bitfile) ;
  set_bitfile("head.tmp",1);
  do_targets() ;
  do_target_dict();
  (void)fprintf(bitfile, "TeXDict begin\n") ;
  (void)fprintf(bitfile, "%%%%EndSetup\n") ;
  fclose(bitfile) ;
  open_output() ;
  noprocset = 1 ;
  removecomments = 0;
  copyfile("head.tmp") ;
  copyfile("body.tmp") ;
}

void set_bitfile(s, mode)
char *s ;
int mode ;
{  
if ((bitfile=fopen(s, mode ? "a" : "w"))==NULL) {
   error(s) ;
   error("!couldn't open file") ;
  }
}

void vertical_in_hps() { 
  Rect_list *rl ;
  /*printf("in vertical_in_hps") ; */
  if (current_type == NAME) return; /* Handle this case later */
  if (!current_rect_list) {
    current_rect_list = (Rect_list *)malloc(sizeof(Rect_list)) ;
    current_rect_list->next = NULL ;
    }
  else {
    rl = (Rect_list *)malloc(sizeof(Rect_list)) ;
    rl->next = current_rect_list ;
    current_rect_list = rl ;
   }
    current_rect_list->rect.llx = dvi_to_hps_conv(hh + hoff,HORIZONTAL) ; 
    current_rect_list->rect.lly = dvi_to_hps_conv(vv + voff,VERTICAL)-FUDGE ;
    current_rect_list->rect.urx = dvi_to_hps_conv(hhmem, HORIZONTAL) ;
    current_rect_list->rect.ury = dvi_to_hps_conv(vvmem, VERTICAL)-FUDGE ; 
  
if (POPPED) start_new_box() ; 
}

void print_rect_list() {
  Rect_list *rl ;
  
  for(rl = current_rect_list ; rl != NULL ; rl = rl -> next) {
   /* printf("Rectangle is %.0f, %.0f, %.0f, %.0f\n", rl->rect.llx, rl->rect.lly, 
            rl->rect.urx, rl->rect.ury) ; */
    free(rl) ;
    } 
} 

void end_current_box() { 
  Hps_link *nl ;

  POPPED = TRUE ;
  HREF_COUNT-- ;
  if (current_type == HREF && current_name[0] != '#') {
	 if ((nl = lookup_link(current_name, current_type)->defn)) { 
	   nl->rect.urx = dvi_to_hps_conv(hhmem + hoff,HORIZONTAL) ;
	   nl->rect.ury = dvi_to_hps_conv(vvmem + voff, VERTICAL)-FUDGE+12.0 ; 
	    stamp_external(current_name,nl); /* Broken lines ? */
	  } else {
	    error("!Null lookup");
	    }
    } else {
    if ((nl = lookup_link(current_name, current_type)->defn)) { 
    nl->rect.urx = dvi_to_hps_conv(hhmem + hoff,HORIZONTAL) ;
    nl->rect.ury = dvi_to_hps_conv(vvmem + voff, VERTICAL)-FUDGE+12.0 ; 
      if (current_type) {
         stamp_hps(nl) ; /* Put link info right where special is */
         }
       } else {
           error("!Null lookup");
           }
      /* printf("Ended box %.0f, %.0f, %.0f, %.0f\n", nl->rect.llx, \
       nl->rect.lly,nl->rect.urx, nl->rect.ury) ; */
      }
}

void start_new_box() {
  POPPED = FALSE ; 
  do_link(current_name, current_type) ;
}
#else
int no_hypertex ; /* prevent an empty file */
#endif
