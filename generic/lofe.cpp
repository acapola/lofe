
#include "lofe_internals.h"
#include <stdio.h>

int main(int argc, char *argv[]){
	uint8_t key_bytes[]       = {0xC5, 0xE6, 0x67, 0xEE, 0x10, 0x97, 0x19, 0x74, 0xDA, 0xC5, 0x52, 0x65, 0x26, 0x01, 0x77, 0x05};
	uint8_t key_tweak_bytes[] = {0x59, 0xA1, 0x24, 0xFC, 0x02, 0x55, 0x41, 0xB6, 0xDE, 0x38, 0x9A, 0x47, 0xEF, 0x25, 0x90, 0xDF};
	int res;
    if(argc<3){
        printf("LOFE: Local On-the-Fly Encryption tool\n");
        printf("2015, Sebastien Riou, lofe@nimp.co.uk\n");
        printf("\n");
        printf("Usage:\n");
        printf("lofe <encrypted_files_path> <mount_point>\n");
        printf("\n");
        exit(-1);
    }
	char *encrypted_files_path=argv[1];
    char *mount_point=argv[2];
    
	int aes_self_test_result = aes_self_test128();
    if(aes_self_test_result){
        printf("aes self test failed with error code: %d\n",aes_self_test_result);
        exit(-1);
    }
	
	//secret key
    lofe_load_key((int8_t*)key_bytes, (int8_t*)key_tweak_bytes);
        
    res = lofe_start_vfs(encrypted_files_path, mount_point);
	return res;
}
