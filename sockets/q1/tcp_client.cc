#include<bits/stdc++.h>
#include<unistd.h>
#include<netinet/in.h>

using namespace std;

string convert(int num){
    string ans = "";
    for(int i=0;i<8;i++){
        if(num & (1<<i)) ans = "1" + ans;
        else{
            ans = "0" + ans;
        }
    }
    return ans;
}

int interpret(string& inp){
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
    int ans = 0;
    for(int i=0;i<8;i++){
        if(inp[i] == '1') ans += (1<<i);
    }
    return -1*(ans);
    
}


int main(){
    int client = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(8080);
    address.sin_addr.s_addr = INADDR_ANY;

    connect(client, (struct sockaddr*)&address, sizeof(address));

    int input;
    cout<<"enter the number: (between 0 and 127)"<<endl;
    cin>>input;
    // first client side conversion
    string binary_string = convert(input);
    cout<<"converted binary: "<<binary_string<<endl;
    int bl = binary_string.size();
    //sending to client
    send(client, &bl, sizeof(int), 0);
    send(client,binary_string.c_str(),bl,0);
    cout<<"binary string sent to the server successfully"<<endl;

    int al;
    read(client,&al,sizeof(int));
    char buffer[1024] = {0};
    read(client,buffer,al);
    string twocomp(buffer,al);
    cout<<"the two's complement from the server is: "<<twocomp<<endl;
    cout<<"the interpreted number is: "<<interpret(twocomp)<<endl;

    close(client);
    return 0;

}