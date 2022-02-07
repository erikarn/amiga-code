/* FIXOBJ : cleanup routine for Amiga Object files (c) 1985 John Hodgson

   Note : FIXOBJ attempts to remove last record filler by truncating all
   information past the last HUNK_END, providing the HUNK_END sequence
   is within the last 128-byte record. The algorithm used here could be
   thwarted if end-of-file filler happens to contain a bogus HUNK_END, or
   if a particular load file ends with anything besides HUNK_END.
*/

#include <lattice/stdio.h>

/* constant definitions */

#define MAX16 (0xffff)
#define REVHUNK_END (0xf203)
#define BYTEBITS (8)
#define ENDOFFSET (2)

main(argc,argv)
int argc;
char *argv[];
{
  FILE *rdptr,*wrtptr,*fopen();
  int pos;
  unsigned int media;
  long ctr;


  if (argc!=3) {
    printf("Usage : fixobj srcfile destfile\n");
    exit(0);
  }
  if ((rdptr=fopen(argv[1],"r"))==NULL) {
    printf("Error opening file %s for reading.\n",argv[1]);
    exit(0);
  }

/* search last record backwards for HUNK_END combo in reverse order */

  media=0;
  for (ctr=-1;ctr>-128;ctr--) {

    /* simulate a 16-bit shift register for easy hunk check */

    media=(media<<BYTEBITS)&MAX16;
    fseek(rdptr,ctr,ENDOFFSET);
    media|=getc(rdptr);
    if (media == REVHUNK_END) break;
  }
  if ((media!=REVHUNK_END) || ctr==-2) {
    (ctr==-2) ? printf("No corrections made!\n"):
                printf("Invalid load format!\n");
    fclose(rdptr);
    exit(0);
  }
  if ((wrtptr=fopen(argv[2],"w"))==NULL) {
    printf("Error opening file %s for writing.\n",argv[2]);
    fclose(rdptr);
    exit(0);
  }

/* duplicate file up to HUNK_END, inclusive */

  pos=ftell(rdptr)+1;
  rewind(rdptr);

  for (ctr=0;ctr<pos;ctr++) putc(getc(rdptr),wrtptr);

  fclose(rdptr); fclose(wrtptr);
}
