#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

/*
1. ��⵽ͼ��仯������
2. ��⵽�ⲿ�жϣ�����
3. 3�������������ͼ��仯20�Σ�����ͣ2Сʱ��2Сʱ���ڿ���
*/
/*define globe variable*/

/*play_pid:��ǰ���ŵ�MP3�ӽ���ID*/
unsigned int play_pid = 0;

/*gradchild:��ǰ���ŵ�MP3���ӽ���ID*/
unsigned int gradchild = 0;

unsigned int play_flag;

/*�����ڴ��������
sharemem:
 byte1:���ӽ���ID��
 byte2:�Ƿ���MP3���ű�ʶplay_flag_2
 //byte3:MP3���Ŵ���
*/
int shmid;
char *p_addr;

#define PERM S_IRUSR|S_IWUSR 



/*��������������,song1ͼ��仯��������
song2�ⲿ�жϱ�������*/
char *song1="11.mp3";
char *song2="22.mp3";
//char *song="234.mp3";


/*��ʱ��ʱ��Ϊ3����*/
#define THREE_ALARM 3*60
/*˯��ʱ��Ϊ2Сʱ*/
#define SLEEP_TIME 2*60*60
#define  CPM_CNT 20
int threemin_alarm = 1;

static int con_cnt=0;
int sleep_flag = 0;
unsigned int time_tmp;

#define max(flag) (flag) >1 ? "pic":"key"
//#define DEBUG
int alarm_flag;

static int pic_cnt;
int cnt_fd;
/*����ͼ��仯����������2������Ϊ��ͼ��仯*/
#define COMPARE_CNT 5


/*************************************************
Function name: count_pic
Called by		 : ����main
Parameter    : void
Description	 : ����ͼƬ�仯��
Return		   : int
Autor & date : ykz 10.4.25
**************************************************/
int count_pic(void)
{
	int fd,ret;
	char *buf;
	buf = (char *)malloc(10);

	system("ls /root/motion | wc -l > count.txt");
	lseek(cnt_fd, 0 ,SEEK_SET);
	ret = read(cnt_fd , buf , 10);
	if(ret)
	  ret = atoi(buf);
	
	free (buf);
	return ret;
}


/*************************************************
Function name: my_func_sleepalarm
Called by		 : ����my_func_3alarm
Parameter    : sing_no
Description	 : 3���������仯20�κ���ʱ2Сʱ��Ļ��Ѻ���
Return		   : void
Autor & date : ykz 10.4.25
**************************************************/
void my_func_sleepalarm(int sign_no)
{

	if( sign_no == SIGALRM){
		printf("Now sleep time finished!start a new work!\n");
		/*��˯�߱�ʾsleep_flagΪ0*/
		sleep_flag = 0;
	}//end sing_no
}


/*************************************************
Function name: my_func_3alarm
Called by		 : ����restart_caculate_play
Parameter    : sing_no
Description	 : ÿ��3���Ӽ��ͼ�������仯�Ƿ񳬹�20��
Return		   : void
Autor & date : ykz 10.4.25
**************************************************/
void my_func_3alarm(int sign_no)
{

	if( sign_no == SIGALRM){

		struct tm *p;
		time_t timep;
		unsigned int second;
	  printf("\n------------------warning------------------\n");
	  printf("[%s]3alarm!con_cnt=%d\n",max(alarm_flag),con_cnt);
	  /*�ж�ͼ���˶������Ƿ񳬹�20��
	   ����ǣ���ϵͳ˯��2Сʱ��
	   ���ǣ���ϵͳ����������һ��3���ӵĶ�ʱ��*/
	  if( con_cnt >= CPM_CNT){
		  if(play_flag){
		    kill(play_pid,SIGKILL);
			kill(gradchild,SIGKILL);
			wait(NULL);
		  }
		  //sleep (1);
		time(&timep);
		p = localtime(&timep);
		second = (p->tm_hour)*60*60 + (p->tm_min) * 60 + p->tm_sec;
		time_tmp=second;

		  printf("Now maybe it is rainning!,system will sleep %d seconds!\n",SLEEP_TIME);
		  con_cnt = 0;
		  /*��˯�߱�ʶsleep_flagΪ1*/
		  sleep_flag = 1;
		  /*ϵͳ˯��2Сʱ*/
		  if(signal(SIGALRM,my_func_sleepalarm)<0)
		    perror("signal");
		  alarm(SLEEP_TIME);
	 }
	 else{
	   printf("Nornal test!not rainning!\n");
	   con_cnt = 0;
	   /*������һ��3���ӵĶ�ʱ��*/
	   if(signal(SIGALRM,my_func_3alarm)<0)
		  perror("signal");
	   alarm(THREE_ALARM);		  
	 }

	  printf("\n------------------warning------------------\n");
	} //end sing_no
}



/*************************************************
Function name: play
Called by		 : ����startplaymp3
Parameter    : void
Description	 : �ӽ��̴������ӽ��̲���MP3
Return		   : void
Autor & date : ykz 10.4.2
**************************************************/
void play(void)
{

	pid_t fd;
	char *c_addr;
	char *song_name;
	int play_flag_gradchild1=1;
	int play_flag_gradchild2=2;
	int i=0;

		/*�������ӽ���*/
		fd = fork();
		if(fd == -1)
		{	
			perror("fork");
			exit(1);
		}
		else if(fd == 0) /*���ӽ��̣�����MP3*/
		{

			printf("\n--------------play mp3----------------\n");
			if(alarm_flag==1)
			  song_name=song1;
			else song_name=song1;
			printf("THIS SONG IS %s\n",song_name);
			/*ʹ��madplay����MP3*/
			execl("/motion/madplay","madplay",song_name,NULL);
			printf("\n\n\n");
		}
		else  /*�ӽ���*/
		{
		
			/*�����ӽ��̵�id���빲���ڴ�*/
			memcpy(c_addr,&fd,sizeof(pid_t));
			
			/*Ŀǰ�ڲ���MP3�������ű�Ǵ��빲���ڴ�*/
			memcpy(c_addr+sizeof(int),&play_flag_gradchild1,4);
			
			/*�ȴ����ӽ��̽�����ֻҪ������
			����play_flag_gradchild2=2,��ʾ����MP3û�в���*/
			if(fd == wait(NULL))
			{
				printf("\n------------------warning------------------\n");
				
			/*ͨ�������ڴ洫��play_flag_gradchild2=2,���������һ������������MP3����*/
					memcpy(c_addr+sizeof(int),&play_flag_gradchild2,4);
					printf("Gradchild normal finish!\n");
		      printf("------------------warning------------------\n");
			}//end if(fd == wait(NULL))
		}

}



/*************************************************
Function name: startplaymp3
Called by		 : ����caculate_play,restart_caculate_play
Parameter    : pid_t *childpid
Description	 : �����̴����ӽ���
Return		   : void
Autor & date : ykz 10.4.2
**************************************************/
void startplaymp3(pid_t *childpid,int flag)
{

		int ret = 0;
	  pid_t cun_pid;
		/*�����ӽ���*/
		cun_pid = fork();
		
		if(cun_pid == -1)
		{	
			perror("son fork");
			exit(1);
		}

		if(cun_pid == 0) /*�ӽ���*/
		  play();
		

		if(cun_pid > 0) /*������*/
		{
			*childpid = cun_pid;
		  sleep(1);  /*�����ӽ�����ִ��*/
		  
		  /*�����ͼ���˶��仯����ȫ�ֱ���con_cnt��1*/
		  if( flag == 2 )
			con_cnt++;
		  
		  printf("\nNow con_cnt=%d\n",con_cnt);
		  /*�����ӽ��̵�pid����������*/
		  memcpy(&gradchild,p_addr,sizeof(pid_t));

		}

}

/*************************************************
Function name: caculate_play
Called by		 : ����key_pic_mp3
Parameter    : int flag
Description	 : ��������ʱ��������������ʱ�䣬���ú���startplaymp3��ʼ�µĲ���
Return		   : int
Autor & date : ykz 10.4.2
**************************************************/
int caculate_play(int flag)
{

	int ret;
	   
	/*kill����ǰ����MP3���ӽ��̣����ӽ���*/
		
		kill(play_pid,SIGKILL);
		kill(gradchild,SIGKILL);
		wait(NULL);
	  
	/*�������ڴ����*/
		memset(p_addr,'\0',1024);

	 startplaymp3(&play_pid , flag);
   return 1;
}


/*************************************************
Function name: restart_caculate_play
Called by		 : ����key_pic_mp3
Parameter    : int flag
Description	 : δ������������ʱ��ʱ�����µ�ͼ������ⲿ�жϼ�⵽ʱ���ú���
Return		   : void
Autor & date : ykz 10.4.2
**************************************************/
void restart_caculate_play(int flag)
{

		int ret;
		/*add 3 minute alarm*/
		if(threemin_alarm){
			if(signal(SIGALRM,my_func_3alarm)<0)
				perror("signal");
			ret = alarm(THREE_ALARM);
			printf("[%s]Start add 3 minute alarm!\n",max(flag));
			threemin_alarm = 0;
		}


#if 1 
    /*�ж��Ƿ����ӽ��̣��������ӽ��̣��������KILL��*/
		if(play_flag){
			kill(play_pid,SIGKILL);
			kill(gradchild,SIGKILL);
			wait(NULL);
			//sleep(1);
		}
#endif
		play_pid = 0;
		gradchild = 0;
		
		memset(p_addr,'\0',1024);
	
	  /*��ʼ����MP3*/
	  startplaymp3(&play_pid,flag);
}



/*************************************************
Function name: key_pic_mp3
Called by		 : ����main
Parameter    : int flag
Description	 : ����⵽���ⲿ�жϣ�����ͼ��仯ʱ��������
Return		   : int
Autor & date : ykz 10.4.2
**************************************************/
int key_pic_mp3(int flag)
{

  printf("------------------------- KEY_PIC_MP3 ----------------------------\n");
	printf("[%s]Now detect ** %s ** have change\n",max(flag),max(flag));
	int ret = 0;
	int over_flag_2; 
	int play_flag_2;

  /*sleep_flag �����жϣ����ϵͳ�Ƿ���2Сʱ��˯��״̬*/
	if(sleep_flag){
		printf("It is rainning!system is sleeping!\n");
		struct tm *p; 
		time_t timep;
		unsigned int second;
		
		time(&timep);
		p = localtime(&timep);
		second = (p->tm_hour)*60*60 + (p->tm_min) * 60 + p->tm_sec;

		printf("summary time:%d seconds,sleep:%d seconds,need sleep:%d seconds\n",
					SLEEP_TIME,second-time_tmp,SLEEP_TIME-(second-time_tmp));

		return 0;
	}
	
	alarm_flag = flag;


	/*��sharemem�ж����Ƿ���MP3���ڲ���״̬��ʶ*/
	memcpy(&play_flag_2,p_addr + sizeof(int),sizeof(int));
//	printf("===>key_pic_mp3,play_flag_2=%d\n",play_flag_2);
	
	play_flag = play_flag_2;
	/*play_flag_2:��ǰ�Ƿ���MP3�ڲ���
	0����ǰû��MP3����
  1:�ӽ��̵�ǰ����MP3����״̬
  2:���ӵ�ǰ����MP3�����������ҵ�ǰû��MP3����*/

	/*��ǰ��MP3�ڲ���*/
	if(play_flag_2 == 1){
			printf("[%s]Yes,have mp3 play\n",max(flag));
			/*����*/
			ret = caculate_play(flag);
	}
	
	/*��ǰ��MP3�ڲ���*/
	else{
			printf("[%s]No, have not mp3 play\n",max(flag));		
		   restart_caculate_play(flag);
	}
	
	
		return ret;
}



/*************************************************
Function name: main
Called by		 : 
Parameter    : void
Description	 : ����������ⰴ���Ƿ��а��£�ͨ��pic.txt���ͼ���Ƿ��б仯
Return		   : int
Autor & date : ykz 10.4.2
**************************************************/
main(void)
{
	
	 int buttons_fd,pic_fd;
	 char pic_buf[1];
	 int key_value;
	 int flag;
	 int ret;
	 int tmp_cnt;
	 /*�豸�ļ��Ĵ�*/
	 buttons_fd = open("/dev/buttons", 0);
	 if(buttons_fd < 0) {
		perror("open device buttons");
		exit(1);
	 }
	 printf("open buttons sucess!\n"); 
	 
	 
	 
	 /*�ļ�pic.txt��¼�Ƿ���ͼ��仯��
	   1����ͼ��仯
	   0��û��ͼ��仯*/  
	 pic_fd = open("pic.txt",O_RDWR | O_CREAT,0666);
	 if(pic_fd < 0) {
		perror("open pic.txt");
		exit(1);
	 }
	 printf("open pic.txt success!\n");
	
	 /*�ļ�count.txt����ͼ�������仯ʱ����¼/root/motion��ͼƬ����*/
	 cnt_fd = open("count.txt",O_RDWR | O_CREAT,0666);
	 if(cnt_fd < 0){
		perror("open count.txt");
		exit(1);
	 }
	 printf("open count.txt success!\n");
	 system("ls > count.txt");
	 
	 /*�����ڴ�����*/
	 	if((shmid = shmget(IPC_PRIVATE,20,PERM))== -1)
			exit(1);
		p_addr = shmat(shmid,0,0);
		memset(p_addr,'\0',1024);
		
		
	/*��ѭ���������ж����ⲿ�жϻ���ͼ��仯*/
	while(1){
	
		/*�ⲿ�жϼ��,������ȡ��ֵ*/
			ret = read(buttons_fd, &key_value, sizeof key_value);
			if (ret != sizeof key_value) 		
					perror("read buttons\n");	 
			else {
				if(key_value){
					printf("====================================================================\n");
					printf("\n\n\n====================================================================\n");
					printf("[type1]button detect,buttons_value: %d\n", key_value);
					/*�ⲿ�жϴ���*/
					key_pic_mp3(1);
				}
			} //end else
		
		
		
		/*ͼ�α仯���,����ͼ��仯ʱmotion�����һ���¼���
		�¼�����Ϊ�ű�/motion/appon���ýű��ȵ���LED�ƣ�
		Ȼ�����ļ�/motion/pic.txtд���ַ�"1"����������ͼ��仯����⵽��
		
		��ȡ�ļ�/motion/pic.txt��һ���ַ�
		 0��û��ͼ��仯
		 1����ͼ��仯*/
		 
		 lseek(pic_fd, 0 ,SEEK_SET);
		 ret = read(pic_fd, pic_buf, 1);

  	 if(ret==1)
		 {
			if(pic_buf[0] == '1'){ /*��ͼ��仯����⵽*/
			 printf("====================================================================\n");
			 printf("\n\n\n====================================================================\n");
			 printf("[type2]pic motion detect!\n");
			 lseek(pic_fd, 0 ,SEEK_SET);
			
			 if((ret = write(pic_fd, "0", 1)))
				  lseek(pic_fd, 0 ,SEEK_SET);
			 
//			 system("rm /root/motion/* -rf");
			 pic_cnt = count_pic();
			 //printf("first pic_cnt=%d\n",pic_cnt);
			 /*ͼ���˶��仯����*/
		 	 key_pic_mp3(2);
		 }
		
			else if(pic_buf[0] == '0'){
			 tmp_cnt = count_pic();
			 if( (tmp_cnt-pic_cnt) > COMPARE_CNT){
				printf("====================================================================\n");
				printf("\n\n\n====================================================================\n");
				printf("[type3]pic motion detect!\n");
			
//				system("rm /root/motion/* -rf");
				pic_cnt = count_pic();
				/*����ͼ���˶��仯����*/
				key_pic_mp3(2);
			 }
			} //end else if
			  
		 }

		sleep (2);
		 	 
	  }//end while
	  close (cnt_fd);
	  close (pic_fd);
	  close (buttons_fd);
	exit(0);	
}