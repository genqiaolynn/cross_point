/******************************************************** 
	* @file    : ����ƥ��-�������ȡ����.cpp
	* @brief   : ������һ��
	* @details : 
	* @author  : duste.Ma
	* @date    : 2021-3-3
*********************************************************/

#include "cv_util.h"
#include "CrossDetector.h"
#include <opencv2/line_descriptor.hpp>
#include <opencv2/ximgproc.hpp>
using namespace cv;
using namespace cv::line_descriptor;
using namespace cv::ximgproc;

using namespace std;

static Mat src;
static Mat tmp;
static Mat tmp1;
static Mat org;
static Rect rect;


static vector<Vec4f> fldLineDetector(Mat & imgGray, bool show, string showName) {
	// Create FLD detector
	// Param               Default value   Description
	// length_threshold    10            - Segments shorter than this will be discarded
	// distance_threshold  1.41421356    - A point placed from a hypothesis line
	//                                     segment farther than this will be
	//                                     regarded as an outlier
	// canny_th1           50            - First threshold for
	//                                     hysteresis procedure in Canny()
	// canny_th2           50            - Second threshold for
	//                                     hysteresis procedure in Canny()
	// canny_aperture_size 3             - Aperturesize for the sobel
	//                                     operator in Canny()
	// do_merge            false         - If true, incremental merging of segments
	//                                     will be perfomred
	int length_threshold = 80;
	float distance_threshold = 1.41421356f;
	double canny_th1 = 50.0;
	double canny_th2 = 50.0;
	int canny_aperture_size = 3;
	bool do_merge = true;
	vector<Vec4f>  fld_lines; fld_lines.clear();
	Ptr<FastLineDetector> fld = createFastLineDetector(length_threshold,
		distance_threshold, canny_th1, canny_th2, canny_aperture_size,
		do_merge);

	fld->detect(imgGray, fld_lines);
	if (show) {
		// Show found lines with FLD
		Mat line_image_fld(imgGray.size(), imgGray.type(), Scalar::all(-1));
		fld->drawSegments(line_image_fld, fld_lines, true);

		resize(line_image_fld, line_image_fld, cv::Size(800, 1131));
		imshow(showName, line_image_fld);
		waitKey(0);
	}
	return fld_lines;
}


static vector<Point2f> get_line_cross_point(const vector<KeyLine> & vecKeyLine,int w,int h) {
	struct KXB	{
		double k;
		double b;
	};
	vector<KeyLine> vecX, vecY;
	vector<KXB> vecKx, vecKy;
	vector<Point2f> vecPts;
	for (size_t i = 0; i < vecKeyLine.size(); i++) {
		KeyLine kTmpLine = vecKeyLine[i];
		if (fabs(kTmpLine.angle) < 8) {
			if (0 == (kTmpLine.startPointX - kTmpLine.endPointX))
				continue;
			vecX.push_back(kTmpLine);
			KXB tmp;
			tmp.k = (kTmpLine.startPointY - kTmpLine.endPointY) / (kTmpLine.startPointX - kTmpLine.endPointX);
			tmp.b = kTmpLine.startPointY - tmp.k*kTmpLine.startPointX;
			vecKx.push_back(tmp);
		}
		else {
			vecY.push_back(kTmpLine);
			KXB tmp;
			if ((kTmpLine.startPointX - kTmpLine.endPointX)<0.1) {
				tmp.k = std::numeric_limits<double>::quiet_NaN(); tmp.b = std::numeric_limits<double>::quiet_NaN();;
			}
			else {
				tmp.k = (kTmpLine.startPointY - kTmpLine.endPointY) / (kTmpLine.startPointX - kTmpLine.endPointX);
				tmp.b = kTmpLine.startPointY - tmp.k*kTmpLine.startPointX;
			}
			vecKy.push_back(tmp);
		}
	}
	// �໥�󽻵�
	for (size_t x=0;x<vecKx.size();x++)	{
		Point2f pt;
		KXB tx = vecKx[x];
		for (size_t y = 0; y < vecKy.size(); y++) {
			KXB ty = vecKy[y];
			if (std::isnan(ty.k)) {
				pt.x = vecY[y].startPointX;
				pt.y = tx.k*pt.x + tx.b;
			}
			else {
				pt.x = (tx.b - ty.b) / (tx.k - ty.k);
				pt.y = pt.x*tx.k + tx.b;
			}
			if(pt.x>0 && pt.x<w && pt.y>0 && pt.y<h)
				vecPts.push_back(pt);
		}
	}

	int x = 0;
	return vecPts;
}

static vector<cv::Rect> get_alternative_boxes(const vector<Point2f> & vecPts, Mat img, int w, int h) {
	vector<cv::Rect> vecBoxes;
	// �ȼ򵥳�һ�� ��ѡ ��ʱ����IOU 
#define RCH 0
	int adWhalf = 30;
	int adHhalf = 30;
	for (size_t i = 0; i < vecPts.size(); i++) {
		Point2f pt = vecPts[i];
		int left = pt.x - adWhalf;
		int top = pt.y - adHhalf;
		int _w = adWhalf * 2;
		int _h = adHhalf * 2;
		Rect box(left, top, _w, _h);
		check_rect(box, w, h);
#if RCH
		rectangle(img, box, cv::Scalar(0), 2, 8, 0);
#endif // RGH

		//. IOU check
		if (vecBoxes.empty()) {
			vecBoxes.push_back(box);
			continue;
		}
		bool insert = false;
		for (size_t m = 0; m < vecBoxes.size(); m++) {
			Rect rect_ocr = vecBoxes[m];
			int endx = max(box.x + box.width, rect_ocr.x + rect_ocr.width);
			int startx = min(box.x, rect_ocr.x);
			int width = box.width + rect_ocr.width - (endx - startx);

			int endy = max(box.y + box.height, rect_ocr.y + rect_ocr.height);
			int	starty = min(box.y, rect_ocr.y);
			int height = box.height + rect_ocr.height - (endy - starty);

			float area = 0.0, iou = 0.0;
			if (width <= 0 || height <= 0) {
				insert = true;
				continue;
			}
			area = width * height*1.0;
			iou = area / float(box.area() + rect_ocr.area() - area);
			if (iou < 0.1) {
				insert = true;
				continue;
			}
			else {
				cv::Rect _box(startx, starty, abs(endx - startx), abs(endy - starty));
				check_rect(_box, w, h);
#if RCH
				rectangle(img, _box, cv::Scalar(0), 2, 8, 0);
#endif // RGH
				vecBoxes[m] = _box;
				insert = false;
				break;
			}
		}
		if(insert)
			vecBoxes.push_back(box);
	}
	return vecBoxes;
}

static vector<KeyLine> get_xy_lines(Mat & imgGray, bool bShow) {
	vector<KeyLine> vecKeyLines; vecKeyLines.clear();
	int length_threshold = 100;
	float distance_threshold = 1.41421356f;
	double canny_th1 = 100.0;
	double canny_th2 = 100.0;
	int canny_aperture_size = 3;
	bool do_merge = true;
	vector<Vec4f>  fld_lines; fld_lines.clear();
	double t = (double)getTickCount();
	Ptr<FastLineDetector> fld = createFastLineDetector(length_threshold,
		distance_threshold, canny_th1, canny_th2, canny_aperture_size,
		do_merge);
	fld->detect(imgGray, fld_lines);
	t = 1000 * ((double)getTickCount() - t) / getTickFrequency();
	cout << "time: " << t << "ms" << endl;
	if (bShow) {
		// Show found lines with FLD
		Mat line_image_fld(imgGray.size(), imgGray.type(), Scalar::all(-1));
		fld->drawSegments(line_image_fld, fld_lines, true);
		//imshow("show", line_image_fld);
		imwrite(R"(F:\data\�ָ��ɶ�λ����Ŀ��������\tmp\show.png)", line_image_fld);
		//waitKey();
	}
	int class_counter = -1;
#define SHOW_ANGLE 0
#if SHOW_ANGLE
	Mat imgCor; cvtColor(imgGray, imgCor, COLOR_GRAY2BGR);
#endif // SHOW_ANGLE

	for (size_t k = 0; k < fld_lines.size(); k++) {
		KeyLine kl;
		Vec4f extremes = fld_lines[k];
		kl.startPointX = extremes[0] * 1; // ��������һ��
		kl.startPointY = extremes[1] * 1;
		kl.endPointX = extremes[2] * 1;
		kl.endPointY = extremes[3] * 1;
		kl.sPointInOctaveX = extremes[0];
		kl.sPointInOctaveY = extremes[1];
		kl.ePointInOctaveX = extremes[2];
		kl.ePointInOctaveY = extremes[3];

		if (kl.startPointX > kl.endPointX) {
			swap(kl.startPointX, kl.endPointX);
			swap(kl.startPointY, kl.endPointY);
		}

		kl.angle = atan2((kl.endPointY - kl.startPointY), (kl.endPointX - kl.startPointX));
		kl.angle = static_cast<float>(-kl.angle * 180 / 3.1415926);
		if (fabs(kl.angle) > 8 && fabs(kl.angle) < 82)
			continue;
#if SHOW_ANGLE
		line(imgCor, Point2f(kl.startPointX, kl.startPointY), Point2f(kl.endPointX, kl.endPointY), Scalar(0, 0, 255), 2, 8, 0);
		//cout << "kl.angle: " << kl.angle << " " << kl.angle+180 << endl;
		//imshow("clor", imgCor);
		//waitKey();
#endif // SHOW_ANGLE


		kl.lineLength = (float)sqrt(pow(extremes[0] - extremes[2], 2) + pow(extremes[1] - extremes[3], 2));

		LineIterator li(imgGray, Point2f(extremes[0], extremes[1]), Point2f(extremes[2], extremes[3]));
		kl.numOfPixels = li.count;

		kl.class_id = ++class_counter;
		kl.octave = 0;
		kl.size = (kl.endPointX - kl.startPointX)*(kl.endPointY - kl.startPointY);
		kl.response = kl.lineLength / max(imgGray.cols, imgGray.rows);
		kl.pt = Point2f((kl.endPointX + kl.startPointX) / 2, (kl.endPointY, kl.startPointY) / 2);
		vecKeyLines.push_back(kl);

	}
#if SHOW_ANGLE
	imshow("line", imgCor);
	waitKey(0);
#endif // SHOW_ANGLE

	return vecKeyLines;
}

static int get_cross_point(const vector<Rect> & vecBoxes, Mat & img,string strSave) {
	double t = (double)getTickCount();
	vector<Cross> vecRes; vecRes.clear();
	CrossDetector crossDetector( 15,  5,  0.5, 4,  15,  180,  20,  80);
	for (size_t t = 0; t < vecBoxes.size(); t++) {
		Rect box = vecBoxes[t];
		vector<Cross> _vecRs;
		crossDetector.detect(img, box, _vecRs);
		vecRes.insert(vecRes.end(), _vecRs.begin(), _vecRs.end());
	}
	Mat imgCor;
	cvtColor(img, imgCor, COLOR_GRAY2BGR);
	for (st i = 0; i < vecRes.size(); i++) {
		Cross ctmp = vecRes[i];
		// ����
		crossDetector.draw(imgCor, ctmp, Scalar(0, 0, 255));
	}

	/// �ٴ�У��

	imwrite(strSave, imgCor);
	t = 1000 * ((double)getTickCount() - t) / getTickFrequency();
	cout << "cross time :" << t << "ms" << endl;
	return 0;
}

void on_mouse(int event, int x, int y, int flags, void* ustc)
{
	static Point pre_pt = { -1,-1 };
	static Point cur_pt = { -1,-1 };
	//cvFont font;
	//cvInitFont(&font, CV_FONT_HERSHEY_SIMPLEX, 0.5, 0.5, 0, 1, CV_AA);//��ʼ������
	char temp[1024];
	src = org.clone();
	if ((event == EVENT_LBUTTONDOWN) && (flags))//����������ʱ
	{
		sprintf(temp, "����������: (%d,%d)\r\n", x, y);//��ʽ���ַ���
		pre_pt = Point(x, y);//��ȡ��ǰ������ֵ
		//cvPutText(src, temp, pre_pt, &font, cvScalar(0, 0, 0, 255));//��ͼ���Ǵ�ӡ�ַ�
		circle(src, pre_pt, 2, Scalar(255, 0, 0), FILLED);//��ͼ���ϻ�Բ
		imshow("src", src);
		//cout << temp << endl;
		//cvCopy(src,tmp);//�����û�У����ǵ�Ŀ��Ͷ�Ŀ�������
	}
	else if ((event == EVENT_MOUSEMOVE) && (flags & EVENT_LBUTTONDOWN))
	{//����ƶ���������������
		sprintf(temp, "����ƶ���������� (%d,%d)", x, y);//��ʽ���ַ���
		cur_pt = Point(x, y);//��ȡ��ǰ������ֵ 
		//cvPutText(src, temp, cur_pt, &font, cvScalar(0, 0, 0, 255));//��ͼ���Ǵ�ӡ�ַ�
		rectangle(src, pre_pt, cur_pt, Scalar(0, 255, 0), 3, 8, 0);//��ͼ���ϻ�����
		imshow("src", src);
		//cout << temp << endl;
	}
	else if (event == EVENT_LBUTTONUP)
	{//����������
		sprintf(temp, "���������� (%d,%d)", x, y);//�����ʽ��
		cur_pt = Point(x, y);//��ȡ��ǰ������ֵ 
		//cvPutText(src, temp, cur_pt, &font, cvScalar(0, 0, 0, 255));//��ͼ���Ǵ�ӡ�ַ�
		circle(src, cur_pt, 2, Scalar(255, 0, 0), FILLED );//��ͼ���ϻ�Բ
		rectangle(src, pre_pt, cur_pt, Scalar(0, 255, 0), 3, 8, 0);//��ͼ���ϻ�����
		imshow("src", src);

		/******************************************************************/
		int width = abs(pre_pt.x - cur_pt.x); //���������� 
		int height = abs(pre_pt.y - cur_pt.y); //����������� 
		if (width == 0 || height == 0)
		{ //��������һ��Ϊ��ʱ���ٴ���
			destroyWindow("dst");
			return;
		}
		tmp1 = Mat(cv::Size(width, height), org.depth(), org.channels());
		
		if (pre_pt.x < cur_pt.x && pre_pt.y < cur_pt.y)
		{
			rect = Rect(pre_pt.x, pre_pt.y, width, height);
		}
		else if (pre_pt.x > cur_pt.x && pre_pt.y < cur_pt.y)
		{
			rect = Rect(cur_pt.x, pre_pt.y, width, height);
		}
		else if (pre_pt.x > cur_pt.x && pre_pt.y > cur_pt.y)
		{
			rect = Rect(cur_pt.x, cur_pt.y, width, height);
		}
		else if (pre_pt.x<cur_pt.x && pre_pt.y>cur_pt.y)
		{
			rect = Rect(pre_pt.x, cur_pt.y, width, height);
		}

		//rect = Rect(464, 299, 470, 116);
		tmp1 = org(rect).clone();
		cout << "box :" << rect.x << " " << rect.y << " " << rect.width << " " << rect.height << endl;

		Mat gray;

		cvtColor(tmp1, gray, COLOR_BGR2GRAY);
		vector<Cross> vecRes; vecRes.clear();
		CrossDetector crossDetector;
		Rect box(0, 0, tmp1.cols, tmp1.rows);
		crossDetector.detect(gray, box, vecRes);
		cout << "got :" << vecRes.size() << " ��" << endl;
		for (st i = 0; i < vecRes.size(); i++) {
			Cross ctmp = vecRes[i];
			// ����
			crossDetector.draw(tmp1, ctmp, Scalar(0, 0, 255));
		}

		destroyWindow("dst");//�����ϴε���ʾͼ�� 
		namedWindow("dst", 1);//�½����� 
		imshow("dst", tmp1); //��ʾ����Ȥ��ͼ�� 
		//cvSaveImage("dst.jpg", tmp1); //�������Ȥͼ�� 
		cout << temp << endl;
	/******************************************************************/
	}
	
}
int main_fudk()
{
	string strPath = R"(F:\data\�ָ��ɶ�λ����Ŀ��������\images\��ѧ\20190508034928060_0001.jpg)";
	src = imread(strPath, 1);//����ͼ��
	tmp = src.clone();//����ͼ����ʱͼ���� 
	org = src.clone();//����ԭʼͼ�� 
	namedWindow("src", 1);//�½�����
	setMouseCallback("src", on_mouse, 0);//ע�������Ӧ�ص�����
	imshow("src", src);//��ʾͼ��
	waitKey(0);//�ȴ��������� 
	destroyAllWindows();//�������д���
	return 0;
}

int main_mia() {

	string strPath = R"(F:\data\�ָ��ɶ�λ����Ŀ��������\images\��ѧ\20190508034928060_0001.jpg)";
	string strSave = R"(F:\data\�ָ��ɶ�λ����Ŀ��������\images\��ѧ\20190508034928060_0001.png)";
	Mat image = imread(strPath);
	Mat imageGray = imread(strPath, 0);

	double t = (double)getTickCount();

	vector<Cross> vecRes; vecRes.clear();
	CrossDetector crossDetector;
	Rect box(0, 0, imageGray.cols, imageGray.rows);
	crossDetector.detect(imageGray, box, vecRes);
	t = 1000 * ((double)getTickCount() - t) / getTickFrequency();
	cout << "got :" << vecRes.size() << " �� ��ʱ:" << t << "ms" <<endl;
	for (st i = 0; i < vecRes.size(); i++) {
		Cross ctmp = vecRes[i];
		// ����
		crossDetector.draw(image, ctmp, Scalar(0, 0, 255));
	}



	imwrite(strSave, image);
	system("pause");
	return 0;
}

int main() {
	string strPath = R"(F:\data\�ָ��ɶ�λ����Ŀ��������\images)";
	string strSave = R"(F:\data\�ָ��ɶ�λ����Ŀ��������\tmp)";
	vector<string> vecDirs; vector<string> vecFiles;
	qqm::io::read_directory(strPath, vecDirs, true);
	for (size_t i = 0; i < vecDirs.size(); i++) {
		cout << vecDirs[i] << endl;
		vector<string> _vecFiles;
		qqm::io::read_directory(vecDirs[i], _vecFiles, true);
		vecFiles.insert(vecFiles.begin(), _vecFiles.begin(), _vecFiles.end());
	}

	double tt = (double)getTickCount();

	for (size_t i = 0; i < vecFiles.size(); i++) {
		cout << "path: " << vecFiles[i] << endl;
		double t = (double)getTickCount();
		string strPath = vecFiles[i];
		Mat img = imread(strPath, 0);
		/// �Զ���ƫ ����ȡ��ƫ����

#if 0
		vector<cv::Rect> _vecBoxes = { cv::Rect(0,0,img.cols,img.rows) };
		int ret3 = get_cross_point(_vecBoxes, img);
		t = 1000 * ((double)getTickCount() - t) / getTickFrequency();
		cout << "�� ��ʱ:" << t << "ms" << endl;
		system("pause");
		continue;
#endif
		
		// step1: ֱ�߼��
		vector<KeyLine> vec4fs = get_xy_lines(img,false);
		// step2: ��ֱ&ˮƽֱ�ߵĽ����ȡ
		vector<Point2f> vecPts = get_line_cross_point(vec4fs, img.cols, img.rows);
#define  SHOW_POINTS 0
#if SHOW_POINTS
		Mat imgCor;
		cvtColor(img, imgCor, COLOR_GRAY2BGR);
		for (size_t t = 0; t < vecPts.size(); t++) {
			circle(imgCor, vecPts[t], 3, cv::Scalar(0, 0, 255), 2, 8, 0);
		}
		imwrite(R"(F:\data\�ָ��ɶ�λ����Ŀ��������\tmp\tmp.png)", imgCor);
#endif
		// step3: ���Ҫ�ж������� IOU CHEECK
		vector<cv::Rect> vecBoxes = get_alternative_boxes(vecPts, img,img.cols, img.rows);
#define  SHOW_BOXES 0
#if SHOW_BOXES
		Mat imgCor1;
		cvtColor(img, imgCor1, COLOR_GRAY2BGR);
		for (size_t t = 0; t < vecBoxes.size(); t++) {
			rectangle(imgCor1, vecBoxes[t],cv::Scalar(0, 0, 255), 2, 8, 0);
		}
		imwrite(R"(F:\data\�ָ��ɶ�λ����Ŀ��������\tmp\tmpbOX.png)", imgCor1);
#endif
		// step3: ������ж�
		char szPath[256];
		sprintf_s(szPath, "%s//%03d.jpg", strSave.c_str(), i);
		int ret = get_cross_point(vecBoxes, img,string(szPath));
		t = 1000 * ((double)getTickCount() - t) / getTickFrequency();
		cout << "�� ��ʱ:" << t << "ms" << endl;
		//system("pause");
	}

	tt = 1000 * ((double)getTickCount() - tt) / getTickFrequency();
	cout << "ƽ�� ��ʱ:" << tt/vecFiles.size() << "ms" << endl;

	system("pause");
	return 0;
}
