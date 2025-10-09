#include<iostream>
#include<unistd.h>
#include<netinet/in.h>
#include<cstring>

using namespace std;

uint32_t compute_checksum(const char* data, int size){
    uint32_t sum = 0;
    for(size_t i=0;i<size;i++){
        sum += static_cast<unsigned char>(data[i]);
    }
    return sum;
}

int main(){
    int server_fd;
    char buffer[1024];

    sockaddr_in server_addr, client_addr;

    server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);

    bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

    FILE* f = fopen("output.txt","wb");

    int chunksize = 1024;
    socklen_t len = sizeof(client_addr);

    while(true){
        char buffer[chunksize];
        int n = recvfrom(server_fd,buffer,chunksize,0,(struct sockaddr*)&client_addr, &len);

        if(n <= 0) break;

        if(strcmp(buffer, "EOF") == 0){
            cout<<"file transfer completed."<<endl;
            break;
        }

        uint32_t sent_cs;
        memcpy(&sent_cs, buffer, sizeof(uint32_t));

        char* chunkdata = buffer + sizeof(uint32_t);
        int datalen = n - sizeof(uint32_t);

        uint32_t checksum = compute_checksum(chunkdata, datalen);

        if(checksum == sent_cs){
            fwrite(chunkdata, 1, datalen, f);
            sendto(server_fd,"ACK",3,0,(struct sockaddr*)&client_addr,len);

        }
        else{
            sendto(server_fd,"NAK",3,0,(struct sockaddr*)&client_addr,len);
        }

    }



    fclose(f);
    close(server_fd);

    return 0;
}