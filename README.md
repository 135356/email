# 135356/email
###### C++发送电子邮件

**示例:**
```
    Smtp smtp(
        25, //服务器端口(默认25)
        "smtp.qq.com", //smtp服务器域名
        "xxx@vip.qq.com", //发件人的邮箱地址
        "xxxxxx", //发件人密码
        "xxx@sina.com", //收件人
        "邮件标题", //主题
        "邮件内容" //内容
    );
```