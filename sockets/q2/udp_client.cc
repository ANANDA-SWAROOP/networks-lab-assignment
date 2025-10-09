#include<iostream>
#include<cstring>
#include<arpa/inet.h>
#include<unistd.h>
#include<vector>

using namespace std;

uint32_t compute_checksum(const char* data, int size){
    uint32_t sum = 0;
    for(size_t i=0;i<size;i++){
        sum += static_cast<unsigned char>(data[i]);
    }
    return sum;
}


int main(){
    int clientfd = socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);

    int chunksize = 1024;
    FILE* f = fopen("input.txt", "rb");

    socklen_t len = sizeof(address);

    char chunk[chunksize];
    while(true){
        int n = fread(chunk,1,chunksize,f);
        if(n <= 0) break;

        uint32_t checksum = compute_checksum(chunk, n);

        vector<char> packet(sizeof(uint32_t) + n);
        memcpy(packet.data(), &checksum, sizeof(uint32_t));
        memcpy(packet.data() + sizeof(u_int32_t),chunk,n);

        while(true){
            sendto(clientfd, packet.data(), packet.size(), 0, (struct sockaddr*)&address, sizeof(address));

            char ack[4];
            int n = recvfrom(clientfd,ack,3,0,(struct sockaddr*)&address,&len);
            ack[n] = '\0';

            if(strcmp(ack, "ACK") == 0) break;
            cout<<"resending packets ... "<<endl;
        }
    }

    sendto(clientfd, "EOF", 3, 0,(struct sockaddr*)&address,sizeof(address));

    fclose(f);
    close(clientfd);

    return 0;
}
