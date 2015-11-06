#include <stdio.h>

void main(){
	FILE *fp = fopen("1.txt", "w");
	int i,j;
	char a = 'z';
	char b = 'x';
	char c = 'y';
	char d = 'm';
	for(i = 0;i < 1024; i ++){
		for(j = 0;j < 512; j ++){
			if( i % 4 == 0){
				fwrite(&a, 1,1,fp); 
			}
			else if (i %4 == 1){
				fwrite(&b,1,1,fp);
			}
			else if(i %4 == 2){
				fwrite(&c, 1,1,fp);

			}
			else{
				fwrite(&d,1,1,fp);
			}
		}
	}
	for(i = 0;i < 1024; i ++){
                for(j = 0;j < 512; j ++){
                        if( i % 4 == 0){
                                fwrite(&d, 1,1,fp);
                        }
                        else if (i %4 == 1){
                                fwrite(&c,1,1,fp);
                        }
                        else if(i %4 == 2){
                                fwrite(&b, 1,1,fp);

                        }
                        else{
                                fwrite(&a,1,1,fp);
                        }
                }
        }

	fclose(fp);
	return;
}
