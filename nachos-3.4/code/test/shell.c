#include "syscall.h"

int
main()
{
    SpaceId newProc;
    OpenFileId input = ConsoleInput;
    OpenFileId output = ConsoleOutput;
    char prompt[2], ch, buffer[60];
    int i;

    prompt[0] = '>';
    prompt[1] = '>';

    while( 1 ){
		Write(prompt, 2, output);
		i = 0;

		do {
		    Read(&buffer[i], 1, input);
		} while( buffer[i++] != '\n' );

		buffer[--i] = '\0';

		if( buffer[0] =='b' && buffer[1]=='a' && buffer[2] =='s' && buffer[3]=='h' && buffer[4]==' '){
			newProc = Exec(buffer+5);
			Join(newProc);
		}
		// if( buffer[0]=='x' && buffer[1]==' ') {
		// 	newProc = Exec(buffer+2);
		// 	Join(newProc);
		// }
		else if( buffer[0] =='p' && buffer[1]=='w' && buffer[2]=='d'){
			Pwd();
		}
		else if( buffer[0] =='l' && buffer[1]=='s'){
			Ls();
		}
		else if( buffer[0] =='c' && buffer[1]=='d' && buffer[2]==' '){
			Cd(buffer+3);
		}
		else if( buffer[0] =='c' && buffer[1]=='f' && buffer[2] ==' ' ){
			Create(buffer+3);
		}
		else if( buffer[0] =='r' && buffer[1]=='f' && buffer[2] ==' ' ){
			Rf(buffer+3);
		}
		else if( buffer[0] =='m' && buffer[1]=='k' && buffer[2] =='d' && buffer[3]=='i' && buffer[4]=='r' &&
			buffer[5] ==' ' ){
			Mkdir(buffer+6);
		}
		else if( buffer[0] =='r' && buffer[1]=='m' && buffer[2] ==' ' ){
			Rm(buffer+3);
		}
		else if( buffer[0] =='e' && buffer[1]=='x' && buffer[2] =='i' && buffer[3]=='t'){
			Exit(0);
		}
		else{
			Print("Command not found\n");
		}
    }
}

