//获取当前目录文件的名称
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
int main(int argc, char * argv[])
{
   struct dirent **namelist;
   int n;

   n = scandir(argv[1], &namelist, NULL, alphasort);
   if (n == -1) 
   {
       perror("scandir");
       exit(EXIT_FAILURE);
   }

   while (n--) 
   {
       printf("%s\n", namelist[n]->d_name);
       free(namelist[n]);
   }
   free(namelist);

   exit(EXIT_SUCCESS);
}
