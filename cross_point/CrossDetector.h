#pragma once

#include "cv_util.h"

/// �ĸ����� 
enum DIR_FLAG {
	DIR_UP = 1,
	DIR_DOWN = 2,
	DIR_LEFT = 4,
	DIR_RIGHT = 8
};

struct Cross{
	int dir;
	int x;
	int y;
	int arg;
};

class CrossDetector{
private:
	/// ��ʶ����㷽����Ϣ
	enum CROSS_DIR_FLAG {
		CROSS_TL = DIR_UP | DIR_LEFT,							
		CROSS_TR = DIR_UP | DIR_RIGHT,							
		CROSS_TLR = DIR_UP | DIR_LEFT | DIR_RIGHT,				
		CROSS_BL = DIR_DOWN | DIR_LEFT,							
		CROSS_BR = DIR_DOWN | DIR_RIGHT,						
		CROSS_BLR = DIR_DOWN | DIR_LEFT | DIR_RIGHT,			
		CROSS_TBL = DIR_UP | DIR_DOWN | DIR_LEFT,				
		CROSS_TBR = DIR_UP | DIR_DOWN | DIR_RIGHT,				
		CROSS_TBLR = DIR_UP | DIR_DOWN | DIR_LEFT | DIR_RIGHT,	
	};
	
	/// �����
	struct _Cross{
		int x;
		int y;
		CROSS_DIR_FLAG dir;
	};

public:
	CrossDetector(int unit_count = 15, double angle = 0, double angle_unit = 0.5, int cross_break = 4, int cross_burr = 15, int grayvalve = 180, int cross_len = 20, int cross_percent = 80);
	~CrossDetector() { ; };
	int detect(const Mat& imageGray, const Rect & rect, std::vector<Cross> & vecCrosses);
	void draw(Mat & imageCor, const Cross & crossDir, Scalar color , int lineW = 2);
private:
	bool isCross( Mat & imageBin, int x, int y, _Cross & cross, const uchar & pix);
	bool isCrossPossable( Mat & imageBin, int x, int y, const uchar & pix);

private:
	double * dx[4];
	double * dy[4];
	int unit_count;
	double angle_unit;
	double angle;
	int total_angle_count;
	std::vector<int> arglist_t;
	std::vector<double> buffer_t;
	double* buffer;
	int* arglist;
	struct {
		//�Ҷȷ�ֵ
		int m_CrossGray;
		//��ⷶΧ
		int m_CrossArg;
		//�����ĳ���
		int m_CrossLen;
		//���ϵ㳤��
		int m_CrossBreak;
		//ë��
		int m_CrossBurr;
		//��Χ�հװٷֱ�
		int m_CrossPercent;
	}options;

};

