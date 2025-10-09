#include<bits/stdc++.h>
#include<netinet/in.h>

using namespace std;

void print(vector<int>& arr){
      for(auto x:arr){
          cout<<x<<" ";
      }
    cout<<endl;
}

string calc_complement(string& inp){
    for(int i=0;i<8;i++){
        if(inp[i] == '1'){
            inp[i] = '0';
        }
        else{
            inp[i] = '1';
        }
    }
    reverse(inp.begin(), inp.end());
    int carry = 1;
    for(int i=0;i<8;i++){
        int val = inp[i] - '0';
        val += carry;
        inp[i] = val%2 + '0';
        carry = val/2;
    }
    reverse(inp.begin(), inp.end());
    return inp;
}


int main(){

    int server_fd, sock;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = INADDR_ANY;

    // allow fast reuse of port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(1);
    }


    //BIND
    bind(server_fd,(struct sockaddr*)&address,sizeof(address));

    //listen
    listen(server_fd,5);

    //accept
    socklen_t alen = sizeof(address);
    sock = accept(server_fd,(struct sockaddr*)&address, &alen);

    int len;
    read(sock,&len,sizeof(int));
    cout<<len<<endl;
    char buffer[1024] = {0};
    read(sock,buffer,len);
    string msg(buffer,len);
    cout<<"recived binary string from client: "<<msg<<endl;
    string ans = calc_complement(msg);
    cout<<"two's complement sent: "<<ans<<endl;
    int al = ans.size();
    send(sock,&al,sizeof(int),0);
    send(sock,ans.c_str(),al,0);
    close(sock);
    close(server_fd);

    return 0;

}