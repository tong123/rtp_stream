#include "dateoritheard.h"
#include "wifiserver.h"
#include "qdatetime.h"
#include "qdebug.h"
#include "h264-enc.h"


#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtppacket.h"
#ifndef WIN32
    #include <netinet/in.h>
    #include <arpa/inet.h>
#else
    #include <winsock2.h>
#endif // WIN32
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>


using namespace jrtplib;



DateOriTheard::DateOriTheard( void * server, QThread *parent)
    : QThread(parent) ,
      mp_date ( NULL),
      mpw_correct_data( NULL )
{
    mpw_correct_data = new unsigned char [ 640 * 480 * 3/2 ];
//    mp_head_data = new unsigned char[ 640*480*3/2 ];
    mp_sps_head = new unsigned char[1000];
    mp_pps_head = new unsigned char[1000];
    mp_yuv_data = new unsigned char [ 640 * 480 * 3/2 ];
    h264_init( );
//    jrtp_test();
    jrtp_init();

    n_sps_size = get_encode_file_head( ( void* )mp_sps_head );
    n_pps_size = get_encode_pps_head( ( void *)mp_pps_head );
}

DateOriTheard::~DateOriTheard()
{
    if ( mpw_correct_data != NULL ){
        delete []mpw_correct_data;
        mpw_correct_data = NULL;
    }
}

//为NALU_t结构体分配内存空间
NALU_t* DateOriTheard::AllocNALU(int buffersize)
{
    NALU_t *n;
    if ((n = (NALU_t*)calloc (1, sizeof (NALU_t))) == NULL){
        printf("AllocNALU: n");
        exit(0);
    }
    n->max_size=buffersize;

    if ((n->buf = (char*)calloc (buffersize, sizeof (char))) == NULL){
        free (n);
        printf ("AllocNALU: n->buf");
        exit(0);
    }

    return n;
}

//释放
void DateOriTheard::FreeNALU(NALU_t *n)
{
    if (n){
        if (n->buf){
            free(n->buf);
            n->buf=NULL;
        }
        free (n);
    }
}

//这个函数输入为一个NAL结构体，主要功能为得到一个完整的NALU并保存在NALU_t的buf中，获取他的长度，填充F,IDC,TYPE位。
//并且返回两个开始字符之间间隔的字节数，即包含有前缀的NALU的长度
int DateOriTheard::GetAnnexbNALU (NALU_t *nalu, unsigned char *buf, unsigned int n_size )
{
    int pos = 0;
    int n_len = 0;
    int info2, info3;
    nalu->startcodeprefix_len=3;//初始化码流序列的开始字符为3个字节

    info2 = FindStartCode2(buf);//判断是否为0x000001
    if(info2 != 1)
    {
        info3 = FindStartCode3(buf);//判断是否为0x00000001
        if (info3 != 1) {
            return -1;
        } else {
            //如果是0x00000001,得到开始前缀为4个字节
//            qDebug()<<"start code 0x00000001";
            pos = 4;
            nalu->startcodeprefix_len = 4;
        }
    }else {
        //如果是0x000001,得到开始前缀为3个字节
        nalu->startcodeprefix_len = 3;
        pos = 3;
    }
//    qDebug()<<"n_len:"<<n_len<<"nalu->startcodeprefix_len:"<<nalu->startcodeprefix_len;
    nalu->len = n_size - nalu->startcodeprefix_len;    //NALU长度，不包括头部。
    memcpy (nalu->buf, &buf[nalu->startcodeprefix_len], nalu->len);//拷贝一个完整NALU，不拷贝起始前缀0x000001或0x00000001
    nalu->forbidden_bit = nalu->buf[0] & 0x80; //1 bit
    nalu->nal_reference_idc = nalu->buf[0] & 0x60; // 2 bit
    nalu->nal_unit_type = (nalu->buf[0]) & 0x1f;// 5 bit
    return n_len;
}

////输出NALU长度和TYPE
void DateOriTheard::dump(NALU_t *n)
{
    if (!n)return;
    //printf("a new nal:");
//    printf("%d\n", n->len);
//    printf(" len: %d\n  ", n->len);
//    qDebug()<<"n-len:"<<n->len;
//    printf("nal_unit_type: %x\n", n->nal_unit_type);
}

int DateOriTheard::FindStartCode2 (unsigned char *Buf)
{
    if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=1) return 0; //判断是否为0x000001,如果是返回1
    else return 1;
}

int DateOriTheard::FindStartCode3 (unsigned char *Buf)
{
    if(Buf[0]!=0 || Buf[1]!=0 || Buf[2] !=0 || Buf[3] !=1) return 0;//判断是否为0x00000001,如果是返回1
    else return 1;
}

void DateOriTheard::checkerror(int rtperr)
{
    if ( rtperr < 0 ) {
        std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
        exit(-1);
    }
}

int DateOriTheard::jrtp_init( )
{
    uint16_t portbase,destport;
    uint32_t destip;
    int status;
    portbase = 9090;
    QString s_dest_ip = QString("192.168.1.100");
    destip = inet_addr( s_dest_ip.toStdString().data() );
    destip = ntohl(destip);
    destport = 9090;

    RTPUDPv4TransmissionParams transparams;
    RTPSessionParams sessparams;

    sessparams.SetOwnTimestampUnit(1.0/90000.0);
    sessparams.SetAcceptOwnPackets(true);
    transparams.SetPortbase(portbase);
    status = sess.Create(sessparams,&transparams);
    checkerror(status);

    RTPIPv4Address addr(destip,destport);
    status = sess.AddDestination(addr);
    checkerror(status);

    sess.SetDefaultPayloadType(96);
    sess.SetDefaultMark(false);
    sess.SetDefaultTimestampIncrement(90000.0 /25.0);
    n = AllocNALU(8000000);//为结构体nalu_t及其成员buf分配空间。返回值为指向nalu_t存储空间的指针

//    int n_head_size = get_encode_file_head( ( void* )mp_head_data );
//    parse_nalu_unit( mp_head_data, n_head_size );
}

void DateOriTheard::jrtp_uninit( )
{
    sess.BYEDestroy(RTPTime(10,0),0,0);
}

void DateOriTheard::run()
{
    m_mutex.tryLock();
    qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    parse_nalu_unit( mp_sps_head, n_sps_size );
    parse_nalu_unit( mp_pps_head, n_pps_size );
//    qDebug()<<"n_head_size: "<<n_head_size;
//    myRgb2YUV( 640, 480, mp_yuv_data, (unsigned char *)mp_date );
    qint64 t2 = QDateTime::currentMSecsSinceEpoch();
    qDebug()<<"t1,t2, t1-t2:"<<t1<<t2<<t2-t1;
    n_data_size = h264_encode( ( void *)mp_yuv_data, (void *)mpw_correct_data, width*height*3/2 );
    qint64 t3 = QDateTime::currentMSecsSinceEpoch();
    qDebug()<<"t2,t3, t2-t3:"<<t2<<t3<<t3-t2;
    parse_nalu_unit( mpw_correct_data, n_data_size );
    qint64 t4 = QDateTime::currentMSecsSinceEpoch();
    qDebug()<<"t3,t4, t3-t4:"<<t3<<t4<<t4-t3;

//    qDebug()<<"n_data_size:"<<n_data_size;
//    qint64 t5 = QDateTime::currentMSecsSinceEpoch();
    qDebug()<<"t1:"<<t1<<"t4:"<<t4;
    m_mutex.unlock();

}

int DateOriTheard::parse_nalu_unit( unsigned char *buf, unsigned int n_size )
{
//    NALU_t *n;
    char* nalu_payload;
    char sendbuf[1500];

    unsigned short seq_num =0;
    int bytes=0;
    int status;
//    qint64 t1 = QDateTime::currentMSecsSinceEpoch();

//    n = AllocNALU(8000000);//为结构体nalu_t及其成员buf分配空间。返回值为指向nalu_t存储空间的指针
//    memset(n->buf, 0, 8000000);
    QString s_tmp = QString( (const char*)buf );
//    printf("buf0: %d\n", buf[0] );
//    qDebug()<<"buf: "<<s_tmp;
    GetAnnexbNALU(n, buf, n_size );//每执行一次，文件的指针指向本次找到的NALU的末尾，下一个位置即为下个NALU的起始码0x000001
//    dump(n);//输出NALU长度和TYPE
//    qDebug()<<"n->len:"<<n->len;
//    //（1）一个NALU就是一个RTP包的情况： RTP_FIXED_HEADER（12字节）  + NALU_HEADER（1字节） + EBPS
//    //（2）一个NALU分成多个RTP包的情况： RTP_FIXED_HEADER （12字节） + FU_INDICATOR （1字节）+  FU_HEADER（1字节） + EBPS(1400字节)
    memset(sendbuf,0,1500);//清空sendbuf；此时会将上次的时间戳清空，因此需要ts_current来保存上次的时间戳值
//    //rtp固定包头，为12字节,该句将sendbuf[0]的地址赋给rtp_hdr，以后对rtp_hdr的写入操作将直接写入sendbuf。
    //  当一个NALU小于1400字节的时候，采用一个单RTP包发送
//     RTPTime starttime = RTPTime::CurrentTime();
//    if(!( n->nal_unit_type==1 || n->nal_unit_type==5 || n->nal_unit_type==7 || n->nal_unit_type==8 ) ) {
//        return -1;
//    }
    qint64 ta = QDateTime::currentMSecsSinceEpoch();
    qint64 tb;

    if( n->nal_unit_type == 5 ) {
        ta = QDateTime::currentMSecsSinceEpoch();
    }

    if(n->len <= MAX_RTP_PACKAGE_SIZE)
    {
        //设置NALU HEADER,并将这个HEADER填入sendbuf[12]
        nalu_hdr =(NALU_HEADER*)&sendbuf[0]; //将sendbuf[12]的地址赋给nalu_hdr，之后对nalu_hdr的写入就将写入sendbuf中；
        nalu_hdr->F = n->forbidden_bit;
        nalu_hdr->NRI=n->nal_reference_idc>>5;//有效数据在n->nal_reference_idc的第6，7位，需要右移5位才能将其值赋给nalu_hdr->NRI。
        nalu_hdr->TYPE=n->nal_unit_type;

        nalu_payload=&sendbuf[1];//同理将sendbuf[13]赋给nalu_payload
        memcpy(nalu_payload,n->buf+1,n->len-1);//去掉nalu头的nalu剩余内容写入sendbuf[13]开始的字符串。
        bytes=n->len ;  //获得sendbuf的长度,为nalu的长度（包含NALU头但除去起始前缀）加上rtp_header的固定长度12字节

        if(n->nal_unit_type==1 || n->nal_unit_type==5 ) {
            status = sess.SendPacket( (void *)sendbuf, n->len, 96, true, 3600 );
        } else {
            status = sess.SendPacket( (void *)sendbuf, n->len, 96, true, 0 );
        }
        if (status < 0) {
                printf("send error\n");
        }
        //发送RTP格式数据包并指定负载类型为96
    } else if( n->len > MAX_RTP_PACKAGE_SIZE ) {
        //得到该nalu需要用多少长度为1400字节的RTP包来发送
        qDebug()<<"n->len > 1400=====";
        int k = 0, last = 0;
        k = n->len / MAX_RTP_PACKAGE_SIZE;//需要k个1400字节的RTP包，这里为什么不加1呢？因为是从0开始计数的。
        last = n->len % MAX_RTP_PACKAGE_SIZE;//最后一个RTP包的需要装载的字节数
        int t = 0;//用于指示当前发送的是第几个分片RTP包
        while(t <= k)
        {
            if(!t)//发送一个需要分片的NALU的第一个分片，置FU HEADER的S位,t = 0时进入此逻辑。
            {
                memset(sendbuf,0,1500);
                fu_ind =(FU_INDICATOR*)&sendbuf[0]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                fu_ind->F = n->forbidden_bit;
                fu_ind->NRI = n->nal_reference_idc >> 5;
                fu_ind->TYPE = 28;  //FU-A类型。

                fu_hdr =(FU_HEADER*)&sendbuf[1];
                fu_hdr->E = 0;
                fu_hdr->R = 0;
                fu_hdr->S = 1;
                fu_hdr->TYPE = n->nal_unit_type;

                nalu_payload = &sendbuf[2];//同理将sendbuf[2]赋给nalu_payload
                memcpy(nalu_payload,n->buf+1,MAX_RTP_PACKAGE_SIZE);//去掉NALU头，每次拷贝1400个字节。
                bytes = MAX_RTP_PACKAGE_SIZE+2;//获得sendbuf的长度,为nalu的长度（除去起始前缀和NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度                                                            14字节
                status = sess.SendPacket((void *)sendbuf,bytes,96,false,0);
                if (status < 0) {
                        printf("send error\n");
                }
                t++;
            }
            //发送一个需要分片的NALU的非第一个分片，清零FU HEADER的S位，如果该分片是该NALU的最后一个分片，置FU HEADER的E位
            else if(k == t)//发送的是最后一个分片，注意最后一个分片的长度可能超过1400字节（当 l> 1386时）。
            {
                memset(sendbuf,0,1500);
                //设置FU INDICATOR,并将这个HEADER填入sendbuf[12]
                fu_ind =(FU_INDICATOR*)&sendbuf[0]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                fu_ind->F=n->forbidden_bit;
                fu_ind->NRI=n->nal_reference_idc>>5;
                fu_ind->TYPE=28;

                //设置FU HEADER,并将这个HEADER填入sendbuf[13]
                fu_hdr = (FU_HEADER*)&sendbuf[1];
                fu_hdr->R = 0;
                fu_hdr->S = 0;
                fu_hdr->TYPE = n->nal_unit_type;
                fu_hdr->E = 1;

                nalu_payload = &sendbuf[2];//同理将sendbuf[14]的地址赋给nalu_payload
                memcpy(nalu_payload,n->buf + t*MAX_RTP_PACKAGE_SIZE + 1,last-1);//将nalu最后剩余的last-1(去掉了一个字节的NALU头)字节内容写入sendbuf[14]开始的字符串。
                bytes = last - 1 + 2;
                status = sess.SendPacket((void *)sendbuf,bytes,96,true,3600);
                t++;
                if( status<0 ) {
                    printf("send error\n");
                }
            }
            //既不是第一个分片，也不是最后一个分片的处理。
            else if(t < k && 0 != t)
            {
                memset(sendbuf,0,1500);
                fu_ind = (FU_INDICATOR*)&sendbuf[0]; //将sendbuf[12]的地址赋给fu_ind，之后对fu_ind的写入就将写入sendbuf中；
                fu_ind->F = n->forbidden_bit;
                fu_ind->NRI = n->nal_reference_idc>>5;
                fu_ind->TYPE = 28;

                fu_hdr =(FU_HEADER*)&sendbuf[1];
                fu_hdr->R = 0;
                fu_hdr->S = 0;
                fu_hdr->E = 0;
                fu_hdr->TYPE = n->nal_unit_type;

                nalu_payload=&sendbuf[2];//同理将sendbuf[14]的地址赋给nalu_payload
                memcpy(nalu_payload, n->buf + t * MAX_RTP_PACKAGE_SIZE + 1,MAX_RTP_PACKAGE_SIZE);//去掉起始前缀的nalu剩余内容写入sendbuf[14]开始的字符串。
                bytes=MAX_RTP_PACKAGE_SIZE + 2;                        //获得sendbuf的长度,为nalu的长度（除去原NALU头）加上rtp_header，fu_ind，fu_hdr的固定长度14字节
                status = sess.SendPacket((void *)sendbuf,bytes,96,false,0);
                if( status<0 ) {
                    printf("send error\n");
                }
                t++;
            }
        }
    }
    if( n->nal_unit_type == 5 ) {
        tb = QDateTime::currentMSecsSinceEpoch();
        qDebug()<<"ta, tb"<<ta<<tb<<tb-ta;
    }
//    RTPTime t = RTPTime::CurrentTime();
//    if( t>RTPTime(40.0) ){
//        qDebug()<<"RTPTime out";
//    }

//        qint64 t2 = QDateTime::currentMSecsSinceEpoch();
//        qDebug()<<t1<<t2;
//    RTPTime delay( 0.00040 );
//    RTPTime::Wait( delay );
//    FreeNALU(n);
    return 0;
}

int DateOriTheard::jrtp_test(void)
{
#ifdef WIN32
    WSADATA dat;
    WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32

    RTPSession sess;
    uint16_t portbase,destport;
    uint32_t destip;
    std::string ipstr;
    int status,i,num;

        // First, we'll ask for the necessary information

    std::cout << "Enter local portbase:" << std::endl;
    std::cin >> portbase;
    std::cout << std::endl;

    std::cout << "Enter the destination IP address" << std::endl;
    std::cin >> ipstr;
    destip = inet_addr(ipstr.c_str());
    if (destip == INADDR_NONE)
    {
        std::cerr << "Bad IP address specified" << std::endl;
        return -1;
    }

    // The inet_addr function returns a value in network byte order, but
    // we need the IP address in host byte order, so we use a call to
    // ntohl
    destip = ntohl(destip);

    std::cout << "Enter the destination port" << std::endl;
    std::cin >> destport;

    std::cout << std::endl;
    std::cout << "Number of packets you wish to be sent:" << std::endl;
    std::cin >> num;

    // Now, we'll create a RTP session, set the destination, send some
    // packets and poll for incoming data.

    RTPUDPv4TransmissionParams transparams;
    RTPSessionParams sessparams;

    // IMPORTANT: The local timestamp unit MUST be set, otherwise
    //            RTCP Sender Report info will be calculated wrong
    // In this case, we'll be sending 10 samples each second, so we'll
    // put the timestamp unit to (1.0/10.0)
    sessparams.SetOwnTimestampUnit(1.0/90000.0);
    sessparams.SetAcceptOwnPackets(true);
    transparams.SetPortbase(portbase);
    status = sess.Create(sessparams,&transparams);
    checkerror(status);

    RTPIPv4Address addr(destip,destport);

    status = sess.AddDestination(addr);
    checkerror(status);
    sess.SetDefaultPayloadType(96);
    sess.SetDefaultMark(false);
    sess.SetDefaultTimestampIncrement(90000.0 /25.0);

    for (i = 1 ; i <= num ; i++)
    {
        printf("\nSending packet %d/%d\n",i,num);

        // send the packet
        status = sess.SendPacket((void *)"1234567890",10,0,false,10);
        checkerror(status);

        sess.BeginDataAccess();

        // check incoming packets
        if (sess.GotoFirstSourceWithData())
        {
            do
            {
                RTPPacket *pack;

                while ((pack = sess.GetNextPacket()) != NULL)
                {
                    // You can examine the data here
                    printf("Got packet !\n");

                    // we don't longer need the packet, so
                    // we'll delete it
                    sess.DeletePacket(pack);
                }
            } while (sess.GotoNextSourceWithData());
        }

        sess.EndDataAccess();

#ifndef RTP_SUPPORT_THREAD
        status = sess.Poll();
        checkerror(status);
#endif // RTP_SUPPORT_THREAD

        RTPTime::Wait(RTPTime(1,0));
    }

    sess.BYEDestroy(RTPTime(10,0),0,0);

#ifdef WIN32
    WSACleanup();
#endif // WIN32
    return 0;
}

void DateOriTheard::myRgb2YUV(int width, int height,  unsigned char *yuv, unsigned char *rgb )
{
    for(int i=0; i<width; ++i )
    {
        for(int j=0; j<height; ++j )
        {
            int B = rgb[j*width*3+i*3];
            int G = rgb[j*width*3+i*3+1];
            int R = rgb[j*width*3+i*3+2];
            int Y = int(16.5+(0.2578*R+0.504*G+0.098*B));
            int U = int(128.5+(-0.148*R-0.291*G+0.439*B));
            int V = int(128.5+(0.439*R-0.368*G-0.071*B));
            yuv[j*width*2+i*2+0] = ( unsigned char )Y;
            if( i%2==1 ) {
                //yuv[j*width*2+i*2-1]=(unsigned char)U;

                //yuv[j*width*2+i*2+1]=(unsigned char)V;
            } else {
                yuv[j*width*2+i*2+1]=(unsigned char)U;
                yuv[j*width*2+i*2+3]=(unsigned char)V;
            }
        }
    }
}

void DateOriTheard::Bitmap2Yuv420p_calc2( unsigned char *destination, unsigned char *rgb, int width, int height )
{
    int image_size = width * height;
    int upos = image_size;
    int vpos = upos + upos / 4;
    int i = 0;

    for( int line = 0; line < height; ++line ) {
        if( !(line % 2) ){
            for( int x = 0; x < width; x += 2 ){
                uint8_t r = rgb[4 * i];
                uint8_t g = rgb[4 * i + 1];
                uint8_t b = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;

                destination[upos++] = ((-38*r + -74*g + 112*b) >> 8) + 128;
                destination[vpos++] = ((112*r + -94*g + -18*b) >> 8) + 128;

                r = rgb[4 * i];
                g = rgb[4 * i + 1];
                b = rgb[4 * i + 2];

                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        } else{
            for( int x = 0; x < width; x += 1 ){
                uint8_t r = rgb[4 * i];
                uint8_t g = rgb[4 * i + 1];
                uint8_t b = rgb[4 * i + 2];
                destination[i++] = ((66*r + 129*g + 25*b) >> 8) + 16;
            }
        }
    }
}



void DateOriTheard::send_date( void * p_date, int width, int height, bool b_org )
{
    mp_date = p_date;
    this->width = width;
    this->height = height;
    this->b_org = b_org;
    Bitmap2Yuv420p_calc2( mp_yuv_data, (unsigned char *)mp_date, 640, 480 );
    memset( mpw_correct_data, 0, sizeof( mpw_correct_data ) );
}
