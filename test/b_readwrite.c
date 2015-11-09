#include <stdio.h>
int main(){
	char buffer[512 * 1024 * 2];
	FILE *fp = fopen("/tmp/C.tar", "rb");
	fseek(fp, 512 * 1024 * 2, SEEK_SET);
	fread(buffer, 512 * 1024 * 2, 1, fp);
	
	FILE *newfile = fopen("tempfile", "wb");
	fwrite(buffer, 1024 * 512 * 2, 1, newfile);

	fclose(fp);
	fclose(newfile);
	return 0;
}
