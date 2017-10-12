#ifndef DATEORITHEARD_H
#define DATEORITHEARD_H

#include "qthread.h"
#include "codec/YFCodec.h"
#include <QMutex>
#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
using namespace jrtplib;
#define H264                    96
#define MAX_RTP_PACKAGE_SIZE    1300
typedef struct
{
    /**//* byte 0 */
    unsigned char csrc_len:4;        /**//* expect 0 */
    unsigned char extension:1;        /**//* expect 1, see RTP_OP below */
    unsigned char padding:1;        /**//* expect 0 */
    unsigned char version:2;        /**//* expect 2 */
    /**//* byte 1 */
    unsigned char payload:7;        /**//* RTP_PAYLOAD_RTSP */
    unsigned char marker:1;        /**//* expect 1 */
    /**//* bytes 2, 3 */
    unsigned short seq_no;
    /**//* bytes 4-7 */
    unsigned  long timestamp;
    /**//* bytes 8-11 */
    unsigned long ssrc;            /**//* stream number is used here. */
} RTP_FIXED_HEADER;

typedef struct {
    //byte 0
    unsigned char TYPE:5;
    unsigned char NRI:2;
    unsigned char F:1;

} NALU_HEADER; /**//* 1 BYTES */

typedef struct {
    //byte 0
    unsigned char TYPE:5;
    unsigned char NRI:2;
    unsigned char F:1;


} FU_INDICATOR; /**//* 1 BYTES */

typedef struct {
    //byte 0
    unsigned char TYPE:5;
    unsigned char R:1;
    unsigned char E:1;
    unsigned char S:1;
} FU_HEADER; /**//* 1 BYTES */

typedef struct
{
    int startcodeprefix_len;      //! 4 for parameter sets and first slice in picture, 3 for everything else (suggested)
    unsigned int len;                 //! Length of the NAL unit (Excluding the start code, which does not belong to the NALU)
    unsigned int max_size;            //! Nal Unit Buffer size
    int forbidden_bit;            //! should be always FALSE
    int nal_reference_idc;        //! NALU_PRIORITY_xxxx
    int nal_unit_type;            //! NALU_TYPE_xxxx
    char *buf;                    //! contains the first byte followed by the EBSP
    unsigned short lost_packets;  //! true, if packet loss is detected
} NALU_t;

class WifiServer;
class DateOriTheard : public QThread
{
public:
    DateOriTheard( void *, QThread *parent = 0);
    ~DateOriTheard();
    void run();
    void *mp_date;
    void send_date( void * p_date, int width, int height, bool);
    void myRgb2YUV(int width, int height,  unsigned char *yuv, unsigned char *rgb );
    void Bitmap2Yuv420p_calc2(unsigned char *destination, unsigned char *rgb, int width, int height );
    void checkerror(int rtperr);
    int jrtp_test(void);
    int jrtp_init( );
    void jrtp_uninit( );
    NALU_t* AllocNALU(int buffersize);
    void FreeNALU(NALU_t *n);
    int GetAnnexbNALU (NALU_t *nalu, unsigned char *buf, unsigned int n_size);
    void dump(NALU_t *n);
    int FindStartCode2 (unsigned char *Buf);
    int FindStartCode3 (unsigned char *Buf);
    int parse_nalu_unit(unsigned char *buf , unsigned int n_size);

private:
    WifiServer *mp_server;
    bool b_org;
    unsigned char   *mpw_correct_data;
    unsigned char   *mp_head_data;
    unsigned char *mp_pps_head;
    unsigned char *mp_sps_head;
    int width;
    int height;
    QMutex m_mutex;
    unsigned char *mp_yuv_data;
    RTPSession sess;
    NALU_HEADER     *nalu_hdr;
    FU_INDICATOR    *fu_ind;
    FU_HEADER       *fu_hdr;
    unsigned int n_sps_size;
    unsigned int n_pps_size;
    unsigned int n_data_size;
    NALU_t *n;
};

#endif // DATEORITHEARD_H
