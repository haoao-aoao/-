// VideoCapture.cpp : Defines the entry point for the console application.
//

// Reference: https://github.com/jpxue/Overwatch-Aim-Assist

#include "stdafx.h"

using namespace ChessEngine;
using namespace cv;

#define GAMEWINDOWNAME		"AI������"
#define ROIWIDTH			864    //864
#define ROIHEIGHT			862
#define WHITE				2//����
#define BLACK				1//����
#define NULL				0//����


// Reference: https://2019.robotix.in/tutorial/imageprocessing/blob_detection/
struct BLOBPOINT
{
	int x, y;
};

struct MYBLOB
{
	int min_x, max_x;
	int min_y, max_y;
	int cen_x, cen_y;
	int n_pixels;
	int ID;
};

Role R;//��ɫ
Position res(0,0);//��һ��AI�����ӵ�����
Position p;//
Position ac[15][15];//1080P������

int BlobNum;
struct MYBLOB *Blob;

Mat Frame;
int p_nums = 0;//��������
char board[15][15]; //��ǰ����
char before_board[15][15];//��һ������
char NewFrame = 0;

bool flag = false; //��ֿ�ʼ���
bool first = false; //����ȿ�ʼ

WzSerialPort SerialPort;//���ڶ���

void GDIRelease(HWND &hwnd, HDC &hdc, HDC &captureDC, HBITMAP &hBmp)
{
	ReleaseDC(hwnd, hdc);
	DeleteObject(hBmp);
	DeleteDC(captureDC);
	DeleteDC(hdc);
}

bool GDICapture()
{
	HWND hwnd = FindWindowA(0, GAMEWINDOWNAME);
	if (hwnd == NULL) {
		printf("ERROR: Game HWND not found!\n");
		return false;
	}

	int sWidth = GetSystemMetrics(SM_CXSCREEN);
	int sHeight = GetSystemMetrics(SM_CYSCREEN);
	if (NewFrame == 0)
		printf("sWidth: %d; sHeight: %d\n", sWidth, sHeight);

	HDC hdc = GetDC(0);  //  retrieves a handle to a device context (DC) for the client area of a specified window or for the entire screen
	HDC captureDC = CreateCompatibleDC(hdc);  // creates a memory device context (DC) compatible with the specified device
	HBITMAP hBmp = CreateCompatibleBitmap(hdc, sWidth, sHeight);  // creates a bitmap compatible with the device that is associated with the specified device context
	HGDIOBJ hOld = SelectObject(captureDC, hBmp);  // selects an object into the specified device context (DC). 

	if (!BitBlt(captureDC, 0, 0, sWidth, sHeight, hdc, 0, 0, SRCCOPY | CAPTUREBLT))	{  // performs a bit-block transfer of the color data corresponding to a rectangle of pixels from the specified source device context into a destination device context
		printf("ERROR: bit-block transfer failed!\n");
		GDIRelease(hwnd, hdc, captureDC, hBmp);
		return false;
	}

	SelectObject(captureDC, hBmp);

	BITMAPINFO bmpInfo = { 0 };
	bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	if (!GetDIBits(hdc, hBmp, 0, 0, NULL, &bmpInfo, DIB_RGB_COLORS)) {  //get bmpInfo
		printf("ERROR: Failed to get Bitmap Info\n");
		GDIRelease(hwnd, hdc, captureDC, hBmp);
		return false;
	}
	bmpInfo.bmiHeader.biCompression = BI_RGB;
	//printf("bmpInfo: %d %d\n", bmpInfo.bmiHeader.biWidth, bmpInfo.bmiHeader.biHeight);  // Captured Screen Width and Height

	if (NewFrame == 0) {
		Frame.create(bmpInfo.bmiHeader.biHeight, bmpInfo.bmiHeader.biWidth, CV_8UC4);
		NewFrame = 1;
	}

	//if (!GetDIBits(hdc, hBmp, 0, bmpInfo.bmiHeader.biHeight, (LPVOID)screen_pixels, &bmpInfo, DIB_RGB_COLORS))	{
	//if (!GetDIBits(hdc, hBmp, 0, bmpInfo.bmiHeader.biHeight, frame.data, &bmpInfo, DIB_RGB_COLORS))	{
	bmpInfo.bmiHeader.biHeight = -sHeight;
	//if (!GetDIBits(hdc, hBmp, 0, CapturedScreenHeight, Frame.data, &bmpInfo, DIB_RGB_COLORS)) {
	if (!GetDIBits(hdc, hBmp, 0, sHeight, Frame.data, &bmpInfo, DIB_RGB_COLORS)) {
		printf("ERROR: Getting the bitmap buffer!\n");
		GDIRelease(hwnd, hdc, captureDC, hBmp);
		return false;
	}

	GDIRelease(hwnd, hdc, captureDC, hBmp);
	return true;
}

//������ж�
char PixelCounter(Mat src)
{

	int nCount_White = 0;//��
	int nCount_Black = 0;//��

	//ͨ������������ͼ������ص�
	Mat_<uchar>::iterator itor = src.begin<uchar>();
	Mat_<uchar>::iterator itorEnd = src.end<uchar>();
	for (; itor != itorEnd; ++itor)
	{
		if ((*itor) > 0)
		{
			//�ף�����ֵ ptr:255
			nCount_White += 1;
		}
		else
		{
			//�ڣ�����ֵ ptr:0
			nCount_Black += 1;
		}

	}
	//����nflag���غڻ�����ظ���
	if (nCount_White >= 256 && nCount_Black == 0)
	{
		//��
		//printf("(��)%d %d\n",nCount_White,nCount_Black);
		p_nums++;
		return BLACK;
	}
	else if(nCount_Black >= 256 && nCount_White == 0)
	{
		//��
		//printf("(��)%d %d\n",nCount_White,nCount_Black);
		p_nums++;
		return WHITE;
	}
	else
	{
		//����
		//printf("(��)%d %d\n",nCount_White,nCount_Black);
		return NULL;
	}

}

void GetBlobs(Mat img)
{
	int i, j, k, l, row, col, id;
	int *pixel_ID;
	struct BLOBPOINT *openlist;
	uchar *data, *data2;
	int head=0, tail=0;

	BlobNum = 0;
	row = img.rows;
	col = img.cols;
	id = 1;
	pixel_ID = (int *)malloc(sizeof(int) * row * col);
	openlist = (struct BLOBPOINT*)malloc(sizeof(struct BLOBPOINT) * row * col);

	for (i=0; i<row; i++) {
		for (j=0; j<col; j++)
			*(pixel_ID + i*col + j) = -1;
	}

	for (i=1; i<row-1; i++) {
		data = img.ptr<uchar>(i);
		for (j=1; j<col-1; j++) {
			if (data[j] == 0 || *(pixel_ID + i*col + j) > -1)
				continue;

			(openlist + tail)->x = j;
			(openlist + tail)->y = i;
			tail++;
			int sum_x=0, sum_y=0, n_pixels=0, max_x=0, max_y=0;
			int min_x=col+1, min_y=row+1;
			while (tail > head) {
				//Dequeue the element at the head of the queue
				struct BLOBPOINT top;
				top.x = (openlist + head)->x;
				top.y = (openlist + head)->y;
				head++;
				*(pixel_ID + top.y*col + top.x) = id;
				n_pixels++;

				//To obtain the bounding box of the blob w.r.t the original image
				min_x = (top.x<min_x) ? top.x : min_x;
				min_y = (top.y<min_y) ? top.y : min_y;
				max_x = (top.x>max_x) ? top.x : max_x;
				max_y = (top.y>max_y) ? top.y : max_y;
				sum_y += top.y;
				sum_x += top.x;

				//Add the 8-connected neighbours that are yet to be visited, to the queue
				for (k=top.y-1; k<=top.y+1; k++) {
					if (k < 0 || k >= row)
						continue;
					data2 = img.ptr<uchar>(k);
					for (l=top.x-1; l<=top.x+1; l++) {
						if (l < 0 || l >= col)
							continue;
						if (data2[l] == 0 || *(pixel_ID + k*col + l) > -1)
						//if (img.at<uchar>(k,l) == 0 || *(pixel_ID + k*col + l) > -1)
							continue;
						*(pixel_ID + k*col + l) = id;
						(openlist + tail)->x = l;
						(openlist + tail)->y = k;
						tail++;
					}
				}
			}

			if (BlobNum >= 1000) // || n_pixels < MinSize || n_pixels > MaxSize || max_x-min_x < MinWidth || max_x-min_x>MaxWidth || max_y-min_y < MinHeight || max_y-min_y > MaxHeight)
				continue;

			(Blob + BlobNum)->min_x = min_x;
			(Blob + BlobNum)->max_x = max_x;
			(Blob + BlobNum)->min_y = min_y;
			(Blob + BlobNum)->max_y = max_y;
			(Blob + BlobNum)->cen_x = sum_x / n_pixels;
			(Blob + BlobNum)->cen_y = sum_y / n_pixels;
			(Blob + BlobNum)->n_pixels = n_pixels;
			(Blob + BlobNum)->ID = id;
			BlobNum++;
			id++;
		}  // for j
	}  // for i

	// print blob information
	//printf("blob num: %d\n", BlobNum);
	
	//for (i=0; i<BlobNum; i++)
		//printf("  %d: min %d,%d; max %d,%d; center %d,%d; width %d; height %d; size %d\n", i, Blob[i].min_x, Blob[i].min_y, Blob[i].max_x, Blob[i].max_y, Blob[i].cen_x, Blob[i].cen_y, Blob[i].max_x-Blob[i].min_x+1, Blob[i].max_y-Blob[i].min_y+1, Blob[i].n_pixels);
		
	// draw blobs on gray image
	Mat result;
	Mat temp;
	img.copyTo(result); // a new copy is created
	int b_cnt = 0;
	int w_cnt = 0;
	for (i=0; i<15; i++) {  // row
		//k = Blob[0].min_y + 1 + i*57;
		k = 31 + 1 + i*57;
		for (j=0; j<15; j++) {   // col
			//l = Blob[0].min_x + 1 + j*57;
			l = 42 + 1 + j*57;
			rectangle(result, Point(l-8, k-8), Point(l+8, k+8), Scalar(127));

			if(BlobNum > 2) continue;
			temp = img(Rect(Point(l-8, k-8), Point(l+8, k+8)));//��ȡ����
			int point = PixelCounter(temp);//ͳ�����̵���
			ac[i][j].x = k+80;
			ac[i][j].y = l+40;
			/*
			if(point == 1){
				b_cnt++;
				printf("����%d:���ROI����(%d, %d)��1080P����(%d,%d)\n",b_cnt,l,k,l+70,k+50);
			}else if(point == 2){
				w_cnt++;
				printf("����%d:���ROI����(%d, %d)��1080P����(%d,%d)\n",w_cnt,l,k,l+70,k+50);
			}else{
			
			}*/
			board[i][j] = point;//��Ž���ά������
		}
	}
	
	imshow("Blob", result);

	free(pixel_ID);
	free(openlist);
}
//��ӡ����
void printfBoard(char arr[15][15])
{
	int i,j;
	for(i=0; i<15; i++) {
		for(j=0; j<15; j++){
			printf("%d ",arr[i][j]);
		}
		printf("\n");
	}
	printf("------------------------------\n");
}


//�ж��ֵ��ĸ���ɫ����
void getRole(Mat img)
{
	int nCount_White = 0;//��
	int nCount_Black = 0;//��

	//ͨ������������ͼ������ص�
	Mat_<uchar>::iterator itor = img.begin<uchar>();
	Mat_<uchar>::iterator itorEnd = img.end<uchar>();
	for (; itor != itorEnd; ++itor)
	{
		if ((*itor) > 0)
		{
			//�ף�����ֵ ptr:255
			nCount_White += 1;
		}
		else
		{
			//�ڣ�����ֵ ptr:0
			nCount_Black += 1;
		}

	}
	if(!first){
		if(nCount_White != 0){
			R = ChessEngine::COMPUTOR;
		}else{
			R = ChessEngine::HUMAN;
		}
	}else {
		if(nCount_White != 0){
			R = ChessEngine::HUMAN;
		}else{
			R = ChessEngine::COMPUTOR;
		}
	}
}

//�Ա�����
Position contrast_array(char arr1[15][15], char arr2[15][15])
{
	for(int i=0; i<15; i++){
		for(int j=0; j<15; j++){
			if(arr1[i][j] != arr2[i][j]){
				Position p;
				p.x = j;
				p.y = i;
				return p;
			}
		}
	}

	return Position(0,0);
}

void init()
{
	p_nums = 0;
	memset(board, EMPTY, 15 * 15 * sizeof(char));
	memset(before_board, EMPTY, 15 * 15 * sizeof(char));
}

void menu()
{
	printf("---------������AI�˵�---------\r\n");
	printf("------------��ѡ��------------\r\n");
	printf("-----A:������� Z:AI����------\r\n");
}

void mouse_move(int x, int y)
{
	mouse_event(MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE, x*65535/1920, y*65535/1080, 0, 0);
	//_sleep(50);
	mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
	mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
	_sleep(500);
}

void port_sendX_Y(int x, int y, int x_micro, int y_micro)
{
	int ret,len;
	char buf[32];

	x = x*200*x_micro;
	y = y*200*y_micro;

	stringstream ss;
	ss<<x; 
	string s1 = "M0 ";
	s1.append(ss.str());

	stringstream ss2;
	ss2<<y; 
	string s2 = " M1 ";
	s2.append(ss2.str());
	s1.append(s2);//ƴ���ַ���
	s1.append("\r");

	len = s1.length();
	strcpy(buf, s1.c_str());
	
	ret = SerialPort.send(buf, len);
	if (ret != len) {
		printf("send error(%d)\n", ret);
	}
	printf("M0 %d M1 %d\r\n",x,y);
}

void port_sendZ(int x, int x_micro)
{
	int ret,len;
	char buf[32];

	x = x*200*x_micro;

	stringstream ss;
	ss<<x; 
	string s1 = "M2 "; 
	s1.append(ss.str());
	s1.append("\r");

	len = s1.length();
	strcpy(buf, s1.c_str());
	
	ret = SerialPort.send(buf, len);
	if (ret != len) {
		printf("send error(%d)\n", ret);
	}
	//printf("Send OK Z:%d len:%d\r\n",x,ret);
	printf("M2 %d\r\n",x);
}

//Z�����+A��ת����
void port_sendZ_A(int x, int y, int x_micro, int y_micro)
{
	int ret,len;
	char buf[32];

	x = x*200*x_micro;
	y = y*200*y_micro;

	stringstream ss;
	ss<<x; 
	string s1 = "M2 "; 
	s1.append(ss.str());

	stringstream ss2;
	ss2<<y; 
	string s2 = " M3 "; 
	s2.append(ss2.str());
	s1.append(s2);//ƴ���ַ���
	s1.append("\r");

	len = s1.length();
	strcpy(buf, s1.c_str());
	
	ret = SerialPort.send(buf, len);
	if (ret != len) {
		printf("send error(%d)\n", ret);
	}
	//printf("Send OK xy=(%d,%d) len:%d\r\n",x,y,ret);
	printf("M2 %d M3 %d\r\n",x,y);
}

int _tmain(int argc, _TCHAR* argv[])
{
	int i, j;
	HWND hwnd = NULL;
	bool run = true;
	Mat ROI;
	Mat ROI_2;
	Mat gray(ROIHEIGHT, ROIWIDTH, CV_8UC1);
	Mat gray_2(20, 60, CV_8UC1);
	uchar *data_rgb, *data_gray;
	unsigned char r, g, b;
	p_nums = 0;
	//VideoWriter writer;

	//writer.open("video.avi", -1, 60, Size(1600, 920)); //writer.open("video.avi", CV_FOURCC('I', 'Y', 'U', 'V'), 60, Size(1600, 920)); // use -1 for fourcc if codec is unknown

	// looking for game window
	hwnd = FindWindowA(0, GAMEWINDOWNAME);
	while (hwnd == NULL) {
		printf(".");
		Sleep(1000);
		hwnd = FindWindowA(0, GAMEWINDOWNAME);
	}
	printf("%s found\n", GAMEWINDOWNAME);

	beforeStart();//׼������
	// open serial port
	SerialPort.open("COM4", 115200, 0, 8, 1);
	// switch to window
	SwitchToThisWindow(hwnd, false);
	Sleep(1000);
	Beep(1000, 250);

	namedWindow("ROI");
	//namedWindow("Gray");
	menu();
	
	// allocate memory for blobs
	Blob = (struct MYBLOB*)malloc(sizeof(struct MYBLOB) * 1000);
	while (run) {
		GDICapture();
		
		if (GetAsyncKeyState(VK_CAPITAL))  // Determines whether a key is up or down at the time the function is called
		{
			run = false;
		}else if(GetAsyncKeyState(0x41)){//"A"
			//printf("��ʼ��");
			init();
			if(flag){
				printf("��ͣ����");
			}else{
				printf("��ʼ����");
			}
			flag = !flag;
			first = true;
			_sleep(500);
		}else if(GetAsyncKeyState(0x5A)){//"Z"
			//printf("��ʼ��");
			init();
			if(flag){
				printf("��ͣ����");
			}else{
				printf("��ʼ����");
			}
			flag = !flag;
			//first = true;
			_sleep(500);
		}
		// define ROI
		ROI = Frame( Rect(70,52, ROIWIDTH, ROIHEIGHT) );  // roi
		ROI_2 = Frame( Rect(1016,891, 60, 20) );  // ��ȡ�Ҳ�
		// generate gray: extract the pixel by color
		for (j=0; j<ROIHEIGHT; j++) {
			data_rgb = ROI.ptr<uchar>(j);
			data_gray = gray.ptr<uchar>(j);
			for (i=0; i<ROIWIDTH; i++) {
				b = data_rgb[i*4];
				g = data_rgb[i*4+1];
				r = data_rgb[i*4+2];
				if ( b == 0 && g == 0 && r == 0)
					data_gray[i] = 255;
				else
					data_gray[i] = 0;
			}  // for i
		}  // for j

		for (j=0; j<20; j++) {
			data_rgb = ROI_2.ptr<uchar>(j);
			data_gray = gray_2.ptr<uchar>(j);
			for (i=0; i<60; i++) {
				b = data_rgb[i*4];
				g = data_rgb[i*4+1];
				r = data_rgb[i*4+2];
				if ( b == 0 && g == 0 && r == 0)
					data_gray[i] = 255;
				else
					data_gray[i] = 0;
			}  // for i
		}  // for j
		// blob detection
		//if(flag)
		GetBlobs(gray);
		getRole(gray_2);
		//printf("�����ϵ���������%d\n",p_nums);
		//writer.write(ROI);
		imshow("ROI", ROI);
		//imshow("ROI_2", gray_2);
		//imshow("Gray", gray);
		if(flag){

			int o = memcmp(board,before_board,15*15);
			p = contrast_array(before_board,board);

			if(R == ChessEngine::COMPUTOR){
				if(p.x == 0){
					if(!first){
						firstPoint(Position(7,7));//������û������ Ĭ�����м�
						mouse_move(ac[7][7].x,ac[7][7].y);
						res.x = 7;
						res.y = 7;
						printf("�µ�һ��");
						port_sendZ(1,8);//z�� ̧��
						_sleep(1*1000);
						port_sendX_Y(7,7,1,2);//�ƶ�x y��
						_sleep(5*1000);
						port_sendZ_A(-1,2,8,32);//��תz��1�� a��˳ת2��
					}else{
						if(o!=0)
						memcpy(before_board,board,sizeof(before_board));
					}
				}else{
					printf("�����λ��(%d,%d)\n",p.x,p.y);
					nextStep(p.x,p.y);
					Position com = getLastPosition();
					printf("com:x:%d y:%d 1080pʵ�����꣺��x:%d,y:%d��\n",com.x,com.y,ac[com.x][com.y].x,ac[com.x][com.y].y);
					mouse_move(ac[com.x][com.y].x,ac[com.x][com.y].y);
					port_sendZ(1,8);//z�� ̧��
					_sleep(1*1000);
					printf("com.x-res.x:%d, com.y-res.y:%d\n",com.x-res.x,com.y-res.y);
					port_sendX_Y(com.x-res.x,com.y-res.y,1,2);//�ƶ�x y��
					_sleep(5*1000);
					port_sendZ_A(-1,2,8,32);//��תz��1�� a��˳ת2��
					res = com;//����AI����λ��
					//system("pause");
				}
			}else{
				if(o!=0){
					//printf("����");
					memcpy(before_board,board,sizeof(before_board));
				}
			}
		}
		waitKey(16);
		//system("pause");
	}
	//writer.release();
	free(Blob);
	Beep(1500, 500);
	return 0;
}