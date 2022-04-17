#ifndef __SMTP_H__
#define __SMTP_H__

#ifdef _MSC_VER
    #include <winsock2.h>
    #pragma comment(lib, "Ws2_32.lib")
#else
    #include <unistd.h>
    #include <cerrno>
    #include <utility>
    #include <sys/fcntl.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <net/if.h>
    #include <sys/ioctl.h>
#endif
#include <cstring>

class Smtp{
    int s_port_;
    std::string s_domain_;
    std::string own_user_;
    std::string own_password_;
    std::string other_target_mail_;
    std::string email_title_;
    std::string email_content_;
    std::string type_;

    int socket_{};
    char r_buf[0xFFF]{}; //接收到的服务器返回内容
    //建立连接
    int connectF(){
        //创建TCP套接字
        socket_ = socket(AF_INET,SOCK_STREAM,0);
        if(socket_ == -1){
            perror("套接字创建失败");
            return -1;
        }
        //获取对应域名的主机信息
        hostent *host_info = gethostbyname(s_domain_.c_str());
        if(host_info == nullptr) {
            perror("获取对应域名的主机信息失败");
            return -1;
        }

        //是否IPv4
        if(host_info->h_addrtype != AF_INET){
            perror("创建结构体失败");
            return -1;
        }

        //服务器地址结构体
        struct sockaddr_in addr_a{};
        socklen_t addr_len = sizeof(struct sockaddr_in);
        addr_a.sin_family = AF_INET;
        addr_a.sin_addr.s_addr = *((unsigned long*)host_info->h_addr_list[0]);
        addr_a.sin_port = htons(s_port_);

        //连接,发送客户端连接请求
        if(connect(socket_,(struct sockaddr *)&addr_a,addr_len) == -1){
            perror("连接失败");
            return -1;
        }
        return 0;
    }
    static std::string enBase64(const char *src_str){
        //定义base64编码表
        char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        //经过base64编码后的字符串长度
        size_t src_str_size = strlen(src_str), res_str_size{};
        if(src_str_size % 3 == 0){
            res_str_size = src_str_size / 3 * 4;
        }else{
            res_str_size = (src_str_size / 3 + 1) * 4;
        }
        int i, j;
        std::string res_str{};
        //char *res_str = new char[sizeof(unsigned char) * res_str_size + 1]; //内部申请的栈空间要在外部释放容易忘记
        for(i = 0, j = 0; i < res_str_size - 2; j += 3, i += 4){ //以3个8位字符为一组进行编码
            res_str += base64_table[src_str[j] >> 2]; //取出第一个字符的前6位并找出对应的结果字符
            res_str += base64_table[(src_str[j] & 0x3) << 4 | (src_str[j + 1] >> 4)]; //将第一个字符的后位与第二个字符的前4位进行组合并找到对应的结果字符
            res_str += base64_table[(src_str[j + 1] & 0xf) << 2 | (src_str[j + 2] >> 6)]; //将第二个字符的后4位与第三个字符的前2位组合并找出对应的结果字符
            res_str += base64_table[src_str[j + 2] & 0x3f]; //取出第三个字符的后6位并找出结果字符
        }
        //处理最后一组数据
        switch(src_str_size % 3){
            case 1:
                res_str[i - 2] = '=';
                res_str[i - 1] = '=';
                break;
            case 2:
                res_str[i - 1] = '=';
                break;
        }
        return res_str;
    }
public:
    Smtp(
            int s_port, //服务器端口
            std::string s_domain, //smtp服务器域名
            std::string own_user, //自己的邮箱地址
            std::string own_password, //密码
            std::string other_target_mail, //目的邮件地址
            std::string email_title, //主题
            std::string email_content, //内容
            std::string type={} //邮件类型html、text
    ):s_port_(s_port),s_domain_(std::move(s_domain)),own_user_(std::move(own_user)),own_password_(std::move(own_password)),other_target_mail_(std::move(other_target_mail)),email_title_(std::move(email_title)),email_content_(std::move(email_content)),type_(std::move(type)){
        #ifdef _MSC_VER
            WORD wVersionRequested;
            WSADATA wsaData;
            int err;
            wVersionRequested = MAKEWORD(2, 1);
            err = WSAStartup(wVersionRequested, &wsaData);
        #endif
        if(SMTP() < 0){
            perror(r_buf);
        }
    }
    ~Smtp(){
        #ifdef _MSC_VER
            closesocket(socket_);
            WSACleanup();
        #else
            close(socket_);
        #endif
    }
public:
    int SMTP(){
        //连接
        if(connectF() == -1 || recv(socket_,r_buf,0xFFF,0) == -1){return -1;}
        if(strstr(r_buf, "220") == nullptr){return -2;}

        //HELO 与服务器确认，通知其客户端使用的机器名称，一般邮件服务器不做限定
        std::string s_str = "HELO "+own_user_+"\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "250") == nullptr){return -2;}

        //AUTH 使用AUTH LOGIN与服务器进行登录验证
        r_buf[0]='\0';
        s_str = "AUTH LOGIN\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "334") == nullptr){return -2;}

        //用户名
        r_buf[0]='\0';
        s_str = own_user_.substr(0, own_user_.find('@', 0));
        s_str = enBase64(s_str.c_str());
        s_str += "\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "334") == nullptr){return -2;}

        //密码
        r_buf[0]='\0';
        s_str = enBase64(own_password_.c_str());
        s_str += "\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "235") == nullptr){return -2;}

        //MAIL FROM 发件人信息，填写与认证信息不同往往被定位为垃圾邮件或恶意邮件
        r_buf[0]='\0';
        s_str = "MAIL FROM: <" + own_user_ + ">\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "250") == nullptr){return -2;}

        //RCPT TO 收信人地址
        r_buf[0]='\0';
        s_str = "RCPT TO: <" + other_target_mail_ + ">\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "250") == nullptr){return -2;}

        //DATA 告诉服务器开始发送数据了
        r_buf[0]='\0';
        s_str = "DATA\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "354") == nullptr){return -2;}

        //邮件头
        r_buf[0]='\0';
        s_str = "From: " + own_user_ + "\r\n"; //FROM邮件基本信息：发信人显示信息（此处可以伪造身份，但是非常容易被识别）
        s_str += "To: " + other_target_mail_ + "\r\n"; //TO邮件基本信息：服务器收件人显示信息
        s_str += "Subject: " + email_title_ + "\r\n"; //SUBJECT邮件基本信息：邮件标题，不填写也往往容易被定位为垃圾邮件
        s_str += "Content-Type: multipart/mixed;boundary=qwertyuiop\r\n"; //邮件头类型
        s_str += "\r\n--qwertyuiop\r\n";//分割(区分Content-Type，邮件头类型与邮件类型)
        //邮件内容
        if(type_ == "html"){
            s_str += "content-type:text/html;charset=utf-8\r\n"; //html类型
        }else{
            s_str += "Content-Type: text/plain;charset=utf-8\r\n"; //文本类型
        }
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1){return -1;}
        s_str += "\r\n" + email_content_ + "\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1){return -1;}
        //邮件尾(.表示结束)
        s_str = "\r\n--qwertyuiop--\r\n.\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "250") == nullptr){return -2;}

        //QUIT断开链接
        r_buf[0]='\0';
        s_str = "QUIT\r\n";
        if(send(socket_,s_str.c_str(),s_str.size(),0) == -1 || recv(socket_, r_buf, 0xFFF, 0) == -1){return -1;}
        if(strstr(r_buf, "221") == nullptr){return -2;}
        return 0;
    }
};

#endif // !__SMTP_H__