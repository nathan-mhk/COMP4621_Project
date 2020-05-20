# COMP 4621 Course Project  
> Kong Ming Hin (20510403)  
> Email: mhkongaa@connect.ust.hk  
>
> [Project Report](/Documentations/COMP%204621%20Project%20Report.pdf)  
> [Demo Video](https://hkustconnect-my.sharepoint.com/%3av%3a/g/personal/mhkongaa_connect_ust_hk/EbQoJan3UZdIrkHoVJy07_8BPKCHcgV8n1qJvMWt6KjVUQ?e=lTIyLK)  
> 
> Instruction:  
> 
> Connect to lab machine using:  
> ssh -L 7000:localhost:12345 \<itsc\>@csl2wkXX.cse.ust.hk  
> 
> Build the proxy:  
> gcc proxy.c -std=gnu99 -lpthread -o proxy  
> 
> Before running the proxy:  
> Use SFTP to transfer Block List.txt to the directory of proxy.c. Edit the file on Windows machine beforehand