/* ����˳��� server.c */ 
//WB
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <setjmp.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include "convert.h"

#include "../avc-src-0.14/avc/common/T264.h"

#define SERVER_PORT 8888 
 
T264_t* m_t264;
T264_param_t m_param;
char* m_pSrc;
char* m_pDst;
int* m_lDstSize;
char* m_pPoolData;


#define USB_VIDEO "/dev/video0"
int cam_fd;
struct video_mmap cam_mm;/*��Ƶ�ڴ�ӳ��*/
/*��������ͷ�Ļ�����Ϣ�������豸���ơ�
֧�ֵ������С�ֱ��ʡ��ź�Դ��Ϣ��*/
struct video_capability cam_cap;
/*���ȡ��ԱȶȵȺ�voide_mmap�еķֱ���*/
struct video_picture cam_pic;
struct video_mbuf cam_mbuf;/*����ͷ�洢��������֡��Ϣ*/
struct video_window win;/* �豸�ɼ����ڲ���*/
char *cam_data = NULL;
int nframe;

void read_video(char *pixels,int w, int h)
{
  int ret;
  int frame=0; 
  cam_mm.width = w;
  cam_mm.height = h;
 /* ���ڵ�֡�ɼ�ֻ������ frame=0*/
  cam_mm.frame = 0;
  cam_mm.format=VIDEO_PALETTE_YUV420P; //change by 091215
/*  �����óɹ����򼤻��豸������ʼһ֡ͼ��Ľ�ȡ���Ƿ�������*/
  ret = ioctl(cam_fd,VIDIOCMCAPTURE,&cam_mm);
  if( ret<0 ) {
    printf("ERROR: VIDIOCMCAPTURE\n");
  }
/*  �����жϸ�֡ͼ���Ƿ��ȡ��ϣ��ɹ����ر�ʾ��ȡ���*/
  ret = ioctl(cam_fd,VIDIOCSYNC,&frame);
  if( ret<0 ) {
    printf("ERROR: VIDIOCSYNC\n");
  }
}

void config_vid_pic()
{
  char cfpath[100];
  FILE *cf;
  int ret;
  if (ioctl(cam_fd, VIDIOCGPICT, &cam_pic) < 0) {
    printf("ERROR:VIDIOCGPICT\n");
  }
  /*ͼ��ɼ���ʽ,����V2000ֻ֧��VIDEO_PALETTE_YUV420P*/
  cam_pic.palette = VIDEO_PALETTE_YUV420P; //change by 091215

#if 0
    cam_pic.brightness  = 30464;
    cam_pic.hue = 36000;
    cam_pic.colour = 0;
    cam_pic.contrast = 43312;
    cam_pic.whiteness = 10312;
    cam_pic.depth =         24;
#endif
    cam_pic.brightness  =	30464;
    cam_pic.hue =		111;
    cam_pic.colour =		555;
    cam_pic.contrast =		43312;
    cam_pic.whiteness =		111;
    /*VIDEO_PALETTE_YUV420,bpp=12bit*/
    cam_pic.depth =         12; //bpp == bytes per pixel,change by 091215
    /*��������ͷ������voideo_picture��Ϣ*/
    ret = ioctl( cam_fd, VIDIOCSPICT,&cam_pic );      

    if( ret<0 ) {
      close(cam_fd);
      printf("ERROR: VIDIOCSPICT,Can't set video_picture format\n");
    }
    return;
}


void init_video(int w,int h) /* bpp == bytes per pixel*/
{
  int ret;
  
  /*�豸�Ĵ�*/
  cam_fd = open( USB_VIDEO, O_RDWR );
  if( cam_fd<0 )
    printf("Can't open video device\n");
	/* ʹ��IOCTL����VIDIOCGCAP����ȡ����ͷ�Ļ�����Ϣ���������С�ֱ���*/
  
  ret = ioctl( cam_fd,VIDIOCGCAP,&cam_cap );       /* ����ͷ�Ļ�����Ϣ*/
  if( ret<0 ) {
    printf("Can't get device information: VIDIOCGCAP\n");
  }
  printf("Device name:%s\nWidth:%d ~ %d\nHeight:%d ~ %d\n",cam_cap.name, cam_cap.maxwidth, cam_cap.minwidth, cam_cap.maxheight, cam_cap.minheight);
  if( ioctl(cam_fd,VIDIOCGWIN,&win)<0 ) {
    printf("ERROR:VIDIOCGWIN\n");
  }
  win.x = 0;  //windows�е�ԭ������
  win.y = 0;  //windows�е�ԭ������
  win.width=w; //capture area ����
  win.height=h; //capture area �߶�
  
  /*ʹ��IOCTL����VIDIOCSWIN����������ͷ�Ļ�����Ϣ*/
  if (ioctl(cam_fd, VIDIOCSWIN, &win) < 0) {
    printf("ERROR:VIDIOCSWIN\n");
  }
  
  /*��������ͷvoideo_picture��Ϣ*/
  config_vid_pic();  
  
   /*ʹ��IOCTL����VIDIOCGCAP����ȡ�������ͷ�洢��������֡��Ϣ*/
  ret = ioctl(cam_fd,VIDIOCGMBUF,&cam_mbuf);
  if( ret<0 ) {
    printf("ERROR:VIDIOCGMBUF,Can't get video_mbuf\n");
  }
  printf("Frames:%d\n",cam_mbuf.frames);
  nframe = cam_mbuf.frames;
  
  /*���Ű�����ͷ��Ӧ���豸�ļ�ӳ�䵽�ڴ���*/
  cam_data = (char*)mmap(0, cam_mbuf.size, PROT_READ|PROT_WRITE,MAP_SHARED,cam_fd,0); 
  if( cam_data == MAP_FAILED ) {
    printf("ERROR:mmap\n");
  }
  printf("Buffer size:%d\nOffset:%d\n",cam_mbuf.size,cam_mbuf.offsets[0]);
  InitLookupTable();
  
}


void init_param(T264_param_t* param, const char* file)
{
	int total_no;
	FILE* fd; 
	char line[255];
	int32_t b;
	if (!(fd = fopen(file,"r")))
	{
		printf("Couldn't open parameter file %s.\n", file);
		exit(-1);
	}

	memset(param, 0, sizeof(*param));
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	if (b != 4)
	{
		printf("wrong param file version, expect v4.0\n");
		exit(-1);
	}
	fgets(line, 254, fd); sscanf(line,"%d", &param->width);
	fgets(line, 254, fd); sscanf(line,"%d", &param->height);
	fgets(line, 254, fd); sscanf(line,"%d", &param->search_x);
	fgets(line, 254, fd); sscanf(line,"%d", &param->search_y);
	fgets(line, 254, fd); sscanf(line,"%d", &total_no);
	fgets(line, 254, fd); sscanf(line,"%d", &param->iframe);
	fgets(line, 254, fd); sscanf(line,"%d", &param->idrframe);
	fgets(line, 254, fd); sscanf(line,"%d", &param->b_num);
	fgets(line, 254, fd); sscanf(line,"%d", &param->ref_num);
	fgets(line, 254, fd); sscanf(line,"%d", &param->enable_rc);
	fgets(line, 254, fd); sscanf(line,"%d", &param->bitrate);
	fgets(line, 254, fd); sscanf(line,"%f", &param->framerate);
	fgets(line, 254, fd); sscanf(line,"%d", &param->qp);
	fgets(line, 254, fd); sscanf(line,"%d", &param->min_qp);
	fgets(line, 254, fd); sscanf(line,"%d", &param->max_qp);
	fgets(line, 254, fd); sscanf(line,"%d", &param->enable_stat);
	fgets(line, 254, fd); sscanf(line,"%d", &param->disable_filter);
	fgets(line, 254, fd); sscanf(line,"%d", &param->aspect_ratio);
	fgets(line, 254, fd); sscanf(line,"%d", &param->video_format);
	fgets(line, 254, fd); sscanf(line,"%d", &param->luma_coeff_cost);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_INTRA16x16) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_INTRA4x4) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_INTRAININTER) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_HALFPEL) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_QUARTPEL) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_SUBBLOCK) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_FULLSEARCH) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_DIAMONDSEACH) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_FORCEBLOCKSIZE) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_FASTINTERPOLATE) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_SAD) * b;
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_EXTRASUBPELSEARCH) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->flags |= (USE_SCENEDETECT) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x16P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x8P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x16P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x8P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x4P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_4x8P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_4x4P) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x16B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_16x8B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x16B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &b);
	param->block_size |= (SEARCH_8x8B) * (!!b);
	fgets(line, 254, fd); sscanf(line,"%d", &param->cpu);
	fgets(line, 254, fd); sscanf(line, "%d", &param->cabac);

	// 	fgets(line, 254, fd); sscanf(line,"%s", src_path);
	// 	fgets(line, 254, fd); sscanf(line,"%s", out_path);
	// 	fgets(line, 254, fd); sscanf(line,"%s", rec_path);
	// 	param->rec_name = rec_path;

	fclose(fd);
}


void init_encoder()
{
	//����׼��
	const char* paramfile = "fastspeed.txt";
	/*��ȡfastspeed.txt�ļ���Ϣ*/
	init_param(&m_param, paramfile);
	m_param.direct_flag = 1;
	/*t264����Ĵ�*/
	m_t264 = T264_open(&m_param);
	m_lDstSize  = m_param.height * m_param.width + (m_param.height * m_param.width >> 1);
  	/*����t264��������ݴ�ŵ��ڴ�*/
	m_pDst = (uint8_t*)T264_malloc(m_lDstSize, CACHE_SIZE);
	
	/*�����ڴ����ڴ��һ֡���ݳ��ȵ�����*/
	m_pPoolData = malloc(m_param.width*m_param.height*3/2); 
}
	
void udps_respon(int sockfd,int w,int h) 
{ 
	struct sockaddr_in addrsrc;
	struct sockaddr_in addrdst; 
	int addrlen,n; 
	
	int32_t iActualLen;
	int row_stride = w*3*h/2;

	bzero(&addrdst,sizeof(struct sockaddr_in)); 
	addrdst.sin_family=AF_INET; 
	/*�ͻ���PC��IP��ַ*/
	addrdst.sin_addr.s_addr=inet_addr("172.18.23.26");
	addrdst.sin_port=htons(SERVER_PORT);

	while(1) 
	{ 
		/*���ݵĲɼ�*/
		read_video(NULL,w,h);
		/*�Բɼ���������ͨ��H264����*/
		iActualLen = T264_encode(m_t264, cam_data, m_pDst, row_stride);
		printf("encoded:%d, %d bytes.\n",row_stride,iActualLen); 
		/*frame_num�����֡��*/
		memcpy(m_pPoolData,&m_t264->frame_num,1);
		/*m_pDst����������*/
		memcpy(m_pPoolData+1, m_pDst, iActualLen);
		iActualLen++;
		/*ʹ��UDPЭ�鷢�ͱ��������ݵ��ͷ���*/
		sendto(sockfd,m_pPoolData,iActualLen,0,(struct sockaddr*)&addrdst,sizeof(struct sockaddr_in)); 	
	} 
} 


void free_dev()
{
  printf("free device\n");
  close(cam_fd);
}

/*���������*/
int main(void) 
{ 	
	int sockfd; 
	struct sockaddr_in addr;

	printf("start 2.0...\n"); 

/*�����׽���*/
	sockfd=socket(AF_INET,SOCK_DGRAM,0); 

	if(sockfd<0) 
	{
		printf("0-");
		printf("Socket Error\n"); 
		exit(1); 
	} 

	bzero(&addr,sizeof(struct sockaddr_in)); 
	addr.sin_family=AF_INET; 
	addr.sin_addr.s_addr=htonl(INADDR_ANY); 
	addr.sin_port=htons(SERVER_PORT); 
	/*�׽��ְ�*/
	if(bind(sockfd,(struct sockaddr *)&addr,sizeof(struct sockaddr_in))<0 ) 
	{ 
		printf(stderr,"Bind Error:%s\n",strerror(errno)); 
		exit(1); 
	} 

	/*�ú�����ɱ���ǰ��׼��*/
	init_encoder();

	atexit( &free_dev );
	/*�ɼ�����ǰ�ĳ�ʼ������*/
	init_video(m_param.width,m_param.height);

	/*ʹ��UDPЭ�鷢�Ͳɼ���������*/
	udps_respon(sockfd,m_param.width,m_param.height); 
	
	close(sockfd); 

} 