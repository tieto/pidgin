#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define BUF_SIZE 10

unsigned long load_file( char* filename, char** wavePtr ) {
  HANDLE inHandle;
  unsigned long	waveSize, action;

  /* Open the WAVE file */
  if (INVALID_HANDLE_VALUE != (inHandle = CreateFile( filename, GENERIC_READ,
						     FILE_SHARE_READ, 0, OPEN_EXISTING, 
						     FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, 0))) {
    /* I'm going to skip checking that this is a WAVE file. I'll assume that it is.
     *	Normally, you'd check it's RIFF header.
     */
    
    /* Get the size of the file */
    if(0xFFFFFFFF == (waveSize = GetFileSize(inHandle, 0)) && waveSize) {
      printf("Bad wave file size\n");
      exit(1);
    }

    /* Allocate some memory to load the file */
    if ((*wavePtr = (char *)VirtualAlloc(0, waveSize, MEM_COMMIT, PAGE_READWRITE))) {
      /* Read in WAVE file */
      if (ReadFile(inHandle, *wavePtr, waveSize, &action, 0) && waveSize == action) {
	return waveSize;
      }
      else {
	printf("Error loading WAVE!\r\n");
      }
      
      /* Free the memory */
      VirtualFree(*wavePtr, waveSize, MEM_FREE);
    }
    else {
      printf("Can't get memory!\r\n");
    }
  }
  else {
    printf("Bad WAVE size!\r\n");
  }
  
  /* Close the WAVE file */
  CloseHandle(inHandle);
  return 0;
}

void usage() {
  printf("Usage: wav2h NameOfFile.wav\n");
  exit(1);
}

int main(int argc, char **argv) {
  FILE *f_in=0;
  FILE *f_out=0;
  int res, i, j, ext=0;
  char *wavedata=0;
  int wavesize=0;
  char dataname[100];
  char filename[100];
  char outfile[100];

  if(argc<2 || argc>2) {
    usage();
  }
  
  for(i=0;i<strlen(argv[1]);i++) {
    if( argv[1][i] == '/' || argv[1][i] == ':' ) {
      printf("You must run wav2h from the directory containing the wav files.\n");
      usage();
    }
    else if( argv[1][i] == '.' ) {
      if( i+3 >= strlen(argv[1]) || 
	  !(argv[1][i+1] == 'w' && argv[1][i+2] == 'a' && argv[1][i+3] == 'v') ||
	  i+4 != strlen(argv[1])) {
	printf("Error: wav2h only works with wav files (with wav extension)\n");
	usage();
      }
      dataname[i] = '\0';
      ext=1;
      break;
    }
    else
      dataname[i] = argv[1][i];
  }

  if(!ext) {
    printf("Error: wav2h only works with wav files (with wav extension)\n");
    usage();
  }

  sprintf(filename, ".\\%s", argv[1]);
  sprintf(outfile, ".\\%s.h", dataname);

  if((wavesize = load_file( filename, &wavedata )) == 0) {
    printf("Error loading file to memory\n");
    exit(1);
  }
  f_out = fopen(outfile, "w+");
  if (!f_out) {
    perror("fopen");
    exit(1);
  }

  fprintf(f_out, "static unsigned char %s[] = {\n", dataname);

  for(i=0,j=0;i<wavesize;i++,j++) {
    fprintf(f_out, "%#x, ", wavedata[i] & 0xff);
    if(j==BUF_SIZE) {
      j=0;
      fprintf(f_out, "\n");
    }
  }

  fprintf(f_out,"};\n");
  fclose(f_out);
  VirtualFree(wavedata, wavesize, MEM_FREE);

  return(0);
}
