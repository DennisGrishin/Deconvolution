/*
 * Deconvolution algorithms
 *
 * Dennis Grishin, 2014.
 */

#include "dft.h"
#include "dft.cpp"
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string>
#include <FreeImage.h>
#pragma comment(lib,"FreeImage.lib")
#pragma comment(lib,"FreeImage.dll")

#define IN
#define OUT
#define UNKNOWN 0
#define BMP 1
#define GIF 2
#define JPEG 3
#define PNG 4
#define TIFF 5
#define BYTE unsigned char
#define FOUR_SIDES 1
#define EIGHT_SIDES 2
#define PSF_RANDOM 0
#define PSF_RADIAL 1
#define PSF_LINEAR 2
#define PSF_RANDOM_BLUR 3
#define PSF_RANDOM_PATH 4

struct IMAGE {
	double *map[3]; // ���������� ����� �����������
	int channels; // ���������� �������� �������. 1 - ����������� �����������, 3 - RGB 
	int width; // ������ ����������� � ��������
	int height; // ������ ����������� � ��������
};

struct FOURIER_IMAGE {
	comp *map[3]; // ����������� ���������� �����
	int channels; // ���������� �������� �������. 1 - ����������� �����������, 3 - RGB 
	int width; // ������ ����������� � ��������
	int height; // ������ ����������� � ��������
};

struct COMPLEX_ARRAYS {
	int size;
	comp *arrays[3];
};

/*
 * ������� ������ �����������
 */
IMAGE *createImage(int width, int height, int channels) {
	int i, k; // �������� ������
	int size; // ���������� �������� �����������
	double *map; // ���������� �����
	IMAGE *image; // ��������� �����������

	if (width < 0 || height < 0) {
		printf("createImage: image cannot be of a size (%d, %d)\n", width, height);
		return 0;
	}
	if (channels != 1 && channels != 3) {
		printf("createImage: image cannot contain %d color channels\n", channels);
	}
	image = new IMAGE();
	image->width = width;
	image->height = height;
	image->channels = channels;
	
	size = width*height;
	for (k = 0; k < channels; k++) {
		image->map[k] = new double[size];
		map = image->map[k];
		for (i = 0; i < size; i++) {
			map[i] = 0.0;
		}
	}
	return image;
}


/*
 * ������� ����� ����� �� �����������
 */
IMAGE *copyImage(IMAGE *image) {
	int i, j; // �������� ������
	int w, h; // ������ � ������ �����������
	int size; // ���������� �������� � �����������
	int channels; // ���������� �������� ������� �����������

	if (image == 0) {
		printf("copyImage: cannot copy image, because it's 0/n");
		return 0;
	}
	w = image->width;
	h = image->height;
	size=w*h;
	channels = image->channels;

	IMAGE *new_image = createImage(w, h, channels);
	for (j = 0; j < channels; j++) {
		for (i = 0; i < size; i++) {
			new_image->map[j][i] = image->map[j][i];
		}
	}
	return new_image;
}

/*
 * ������� �����������
 */
void deleteImage(IMAGE *image) {
	int i; // ������� �����
	int channels; // ���������� �������� �������

	if (image == 0) {
		printf("deleteImage: cannot delete image, because it's 0/n");
		return;
	}
	channels = image->channels;
	for (i = 0; i < channels; i++) {
		delete [] image->map[i];
	}
	delete image;
}

/*
 * ���������� ����������� (���������������� ��������������� �������)
 */
IMAGE *generateImage(int w, int h, int channels) {
	IMAGE *image;
	int k, i, size;

	srand(time(NULL));
	size = w*h;
	image = createImage(w, h, channels);
	for (i = 0; i < size; i++) {
		for (k = 0; k < channels; k++) {
			image->map[k][i] = 0.0;
		}
		if (rand()%size < 10) {
			image->map[rand()%3][i] = 1.0;
		}
	}
	return image;
}

/*
 * ���������� ���
 */
IMAGE *generatePSF(int width, int height, int type = PSF_RANDOM) {
	int i, j; // �������� ������
	int a, b; // ���������� � ���������� ���
	int size; // ���������� �������� � �����������
	IMAGE *psf; // ��������� ���
	double lum; // ������������� ������� ������� � �����������
	double *map; // ���������� ����� ��������� ���

	if (width%2 != 1 || height%2 != 1) {
		printf("conv: PSF is of non-standard size (%d, %d)\n", width, height);
	}
	psf = createImage(width, height, 1);
	map = psf->map[0];
	a = width/2;
	b = height/2;
	size = width*height;

	switch(type) {
		case PSF_RANDOM:
			for (i = 0; i < size; i++) {
				map[i] = (double)(rand()%255)/255;
			}
			break;

		case PSF_RADIAL:
			for (i = 0; i < width; i++) {
				for (j = 0; j < height; j++) {
					lum = sqrt((double)((i-a)*(i-a)+(j-b)*(j-b)));
					lum = 1.0 - lum/(a+1);
					if (lum < 0) lum = 0;
					map[i*height + j] = lum;
				}
			}
			break;
		case PSF_LINEAR:
			for (i = b*width + a; i < (b+1)*width; i++) {
				map[i] = 0.5;
			}
			break;
		case PSF_RANDOM_PATH:
		case PSF_RANDOM_BLUR:
		break;
	}
	return psf;
}

/*
 * ������� ������ ��� (����� �������� ���������)
 */
double getPSFDivisor(IMAGE *psf) {
	int w, h; // ������ � ������ ��� � ��������
	int i, j; // �������� ������
	double div; // ����������� ���
	double *map; // ���������� ����� ���

	w = psf->width;
	h = psf->height;
	map = psf->map[0];

	div = 0.0;
	for (i = 0; i < w; i++) {
		for (j = 0; j < h; j++) {
			div += map[i*h + j];
		}
	}
	if (div == 0) {
		printf("getPSFDivisor: psf has only zeros\n");
	}
	return div;
}

/*
 * ��������� �����������, ��������� FreeImage
 */
IMAGE *loadImage(const char *name, int type) {
	IMAGE *result;
	FIBITMAP *bitmap;
	FREE_IMAGE_FORMAT fif;

	switch(type) {
		case 1: 
			fif = FIF_BMP; 
			break;
		case 2: 
			fif = FIF_GIF; 
			break;
		case 3: 
			fif = FIF_JPEG; 
			break;
		case 4: 
			fif = FIF_PNG; 
			break;
		case 5: 
			fif = FIF_TIFF; 
			break;
		default:
			fif = FIF_UNKNOWN;
	}
	if (fif == FIF_UNKNOWN) {
		printf("loadImage: unknown file format: %d\n", type);
	} else {
		bitmap = FreeImage_Load(fif, name, 0); // ��������� ����������� � ������
		if (bitmap) {
			int width, height; // ������ � ������ ������������ ����������� � ��������
			int x, y; // ���������� �������
			bool success; // ������� �� �������� ��� �������
			RGBQUAD color; // ���� �������

			printf("loadImage: image was loaded! Year!\n");
			
			width = FreeImage_GetWidth(bitmap);
			height = FreeImage_GetHeight(bitmap);		
			result = createImage(width, height, 3);

			success = true;
			for (x = 0; x < width; x++) {
				for (y = 0; y < height; y++) {
					success = FreeImage_GetPixelColor(bitmap, x, y, &color);
					result->map[0][width*y + x] = (double)color.rgbRed/255;
					result->map[1][width*y + x] = (double)color.rgbGreen/255;
					result->map[2][width*y + x] = (double)color.rgbBlue/255;
					if (!success) break;
				}
				if (!success) break;
			}
			if (success) {
				printf("loadImage: color maps were formed!\n");
			} else {
				printf("loadImage: couldn\'t get pixel (%d, %d)\n", x, y);
			}
			FreeImage_Unload(bitmap);
		} else {
			printf("loadImage: image was not loaded\n");
			return 0;
		}
	}
	return result;
}

/*
 * ��������� �����������, ��������� FreeImage
 */
void saveImage(IMAGE *image, const char *name, int type) {
	FIBITMAP *bitmap;
	FREE_IMAGE_FORMAT fif;
	
	if (image == 0) {
		printf("saveImage: cannot save image, because it's 0\n");
		return;
	}
	switch(type) {
		case 1: 
			fif = FIF_BMP; 
			break;
		case 2: 
			fif = FIF_GIF; 
			break;
		case 3: 
			fif = FIF_JPEG; 
			break;
		case 4: 
			fif = FIF_PNG; 
			break;
		case 5: 
			fif = FIF_TIFF; 
			break;
		default:
			fif = FIF_UNKNOWN;
	}
	if (fif == FIF_UNKNOWN) {
		printf("saveImage: unknown file format: %d\n", type);
		return;
	}
	
	bitmap = FreeImage_Allocate(image->width, image->height, 24); // �������� ������ ��� �����������
	if (bitmap) {
		int width, height; // ������ � ������ ������������ ����������� � ��������
		int x, y; // ���������� �������
		bool success; // ������� �� �������� ��� �������
		double *red_map, *green_map, *blue_map;
		RGBQUAD color; // ���� �������
		
		printf("saveImage: image was created!\n");

		if (image->channels == 3) {
			red_map = image->map[0];
			green_map = image->map[1];
			blue_map = image->map[2];
		} else {
			red_map = image->map[0];
			green_map = image->map[0];
			blue_map = image->map[0];
		}

		width = image->width;
		height = image->height;
		
		success = true;
		for (x = 0; x < width; x++) {
			for (y = 0; y < height; y++) {
				color.rgbRed = (BYTE)(red_map[width*y + x]*255);
				color.rgbGreen = (BYTE)(green_map[width*y + x]*255);
				color.rgbBlue = (BYTE)(blue_map[width*y + x]*255);
				success = FreeImage_SetPixelColor(bitmap, x, y, &color);
				if (!success) break;
			}
			if (!success) break;
		}
		if (success) {
			printf("saveImage: color maps were writed into bitmap!\n");
			if (FreeImage_Save(fif, bitmap, name)) {
				printf("saveImage: bitmap was successfully saved!\n");
			} else {
				printf("saveImage: bitmap couldn\'t be saved\n");
			}
		} else {
			printf("saveImage: couldn\'t set pixel (%d, %d)\n", x, y);
		}
		FreeImage_Unload(bitmap);
	} else {
		printf("saveImage: image was not created\n");
	}
}

/*
 * ��������� ����������� � �����������
 */
void grayscale(IMAGE *image) {
	int w, h; // ������ � ������ �����������
	int size; // ���������� �������� �����������
	int i; // ������� �����
	double lum; // ������� �������

	if (image->channels == 1) {
		printf("grayscale: image is already grayscale, for what it's worth\n");
		return;
	}
	w = image->width;
	h = image->height;
	size = w*h;

	for (i = 0; i < size; i++) {
		lum = 0.299*(image->map[0][i]) + 0.587*(image->map[1][i]) + 0.114*(image->map[2][i]);
		if (lum > 1.0) lum = 1.0;
		image->map[0][i] = lum;
	}
	image->channels = 1;
	delete [] image->map[1];
	delete [] image->map[2];
}

/*
 * ����������� ����������� 
 */
void inverse(IMAGE *image) {
	int w, h; // ������ � ������ �����������
	int size; // ���������� �������� �����������
	int channels; // ���������� �������� �������
	int i, j; // �������� ������
	double *map; // ���������� ����� �����������

	w = image->width;
	h = image->height;
	size = w*h;
	channels = image->channels;
	for (j = 0; j < channels; j++) {
		map = image->map[j];
		for (i = 0; i < size; i++) {	
			map[i] = 1.0 - map[i];
		}
	}
}

/*
 * ������ ��������, ���������
 */
void laplace(IMAGE *image, int type) {
	int w, h; // ������ � ������ �����������
	int size; // ���������� �������� �����������
	int channels; // ���������� �������� �������
	int i, j, x, y; // �������� ������
	double *map; // ���������� ����� �������������� �����������
	double *buf; // ���������� ����� ��������� �����������
	double lum; // ������� ������� �����������

	w = image->width;
	h = image->height;
	size = w*h;
	channels = image->channels;

	for (j = 0; j < channels; j++) { // ���� �� �������� �������
		printf("laplace: color channel %d\n", j);
		map = image->map[j];
		buf = new double[size];
		for (i = 0; i < size; i++) {
			buf[i] = map[i];
		}
		
		for (x = 1; x < w - 1; x++) {
			for (y = 1; y < h - 1; y++) {
				if (type == (type|FOUR_SIDES)) {
					lum = 5*buf[y*w+x];
					lum -= buf[y*w+x+1] + buf[y*w+x-1] + buf[(y+1)*w+x] + buf[(y-1)*w+x];
					if (lum < 0.0) lum = 0.0;
					if (lum > 1.0) lum = 1.0;
					map[y*w+x] = lum;
				} 
				else {
					lum = 9*buf[y*w+x];
					lum -= buf[y*w+x+1] + buf[y*w+x-1] + buf[(y+1)*w+x] + buf[(y-1)*w+x];
					lum -= buf[(y+1)*w+x+1] + buf[(y+1)*w+x-1] + buf[(y-1)*w+x+1] + buf[(y-1)*w+x-1];
					if (lum < 0.0) lum = 0.0;
					if (lum > 1.0) lum = 1.0;
					map[y*w+x] = lum;
				}
			}
		}
		delete [] buf;
	}
}

/*
 * �������
 */
void _conv(IN double **in_maps, double *h, int channels, int w1, int h1,
		   int w2, int h2, int a, int b, double div, OUT double **out_maps) {
	int k, x, y, i, j; // �������� ������
	double *f, *map; // ���������� ����� �������� ����������� � ��������� �����������
	int border_x, border_y; // ������� ������� ������ ���� ����������� �������
	int index_x, index_y; // ���������� ������� ��������� ����������� � ���. ����. �������
	double sum; // ����� ������� ��� ���������� �������

	for (k = 0; k < channels; k++) {
		f = in_maps[k];
		map = out_maps[k];
		for (x = 0; x < w1; x++) {
			border_x = x - a + w1; // w1 ������������ ��� ���������� ������ �������� %
			for (y = 0; y < h1; y++) { // ������ �� ������ ����� ����������� f
				border_y = y - b + h1; // h1 ������������ ��� ���������� ������ �������� %
				sum = 0;
				for (i = 0; i < w2; i++) {
					for (j = 0; j < h2; j++) { // ������ �� ������� ����� ��� g
						index_x = (border_x + i)%w1;
						index_y = (border_y + j)%h1;
						sum += h[(h2 - j)*w2 - i - 1]*f[index_y*w1 + index_x];
					}
				}
				map[x + y*w1] = sum/div;
			}
		}
		//printf("conv: done with channel %d\n", k);
	}
}

/*
 * ������� ����������� � ���
 */
IMAGE *conv(IMAGE *image, IMAGE *psf) {
	int i, j; // �������� ������
	int w1, h1, w2, h2; // ������� � �������� ����������� � ���
	int a, b; // ���������� � ���������� ���
	int channels; // ���������� �������� �������
	double *h; // ���������� ����� ���
	double div; // ����������� ���
	IMAGE *result; // �������� �����������

	w2 = psf->width;
	h2 = psf->height;

	if (psf->channels > 1) {
		printf("conv: PSF should be a grayscale image\n");
		return 0;
	}
	if (w2%2 != 1 || h2%2 != 1) {
		printf("conv: PSF cannot be of a size (%d, %d)\n", w2, h2);
		return 0;
	}
	channels = image->channels;
	w1 = image->width;
	h1 = image->height;
	a = w2/2; // �������������� ���������� �� ������ psf �� ����
	b = h2/2; // ������������ ���������� �� ������ psf �� ����
	h = psf->map[0];

	// ���������� ����������� ���
	div = getPSFDivisor(psf);
	if (div == 0) {
		return 0;
	}
	result = createImage(w1, h1, channels);
	
	_conv(image->map, h, channels, w1, h1, w2, h2, a, b, div, result->map);

	return result;
}

/*
 * ������� �������� � �������� ����
 */
IMAGE *deconv(IMAGE *image, IMAGE *psf) {
	int w1, h1, w2, h2; // ������� ����������� � ���
	int size1, size2; // ���������� �������� ����������� � ���
	int channels; // ���������� �������� ������� �����������
	int x, y, i, j, k, t; // �������� ������
	int a, b; // ���������� � ���������� ���
	IMAGE *latent; // ����������������� �����������
	double div; // ����������� ��� 
	double *h, *g, *f; // ���������� �����

	w2 = psf->width;
	h2 = psf->height;

	if (psf->channels > 1) {
		printf("deconv: PSF should be a grayscale image\n");
		return 0;
	}
	if (w2%2 != 1 || h2%2 != 1) {
		printf("deconv: PSF cannot be of a size (%d, %d)\n", w2, h2);
		return 0;
	}

	a = w2/2; // �������������� ���������� �� ������ psf �� ����
	b = h2/2; // ������������ ���������� �� ������ psf �� ����
	channels = image->channels;
	w1 = image->width;
	h1 = image->height;
	size1 = w1*h1;
	size2 = w2*h2;

	latent = copyImage(image);
	h = psf->map[0];

	// ���������� ����������� ���
	div = getPSFDivisor(psf);
	if (div == 0) {
		return 0;
	}

	double *A;
	int N, M, NxM; 
	int pix1, pix2;
	int border_x, border_y; // ������� ������� ������ ���� ����������� �������
	int index_x, index_y; // ���������� ������� ��������� ����������� � ���. ����. �������
	double sum; // ����� ������� ��� ���������� �������

	N = size1;
	M = size1 + 1;
	NxM = N*M;
	A = new double[NxM];
	for (i = 0; i < NxM; i++) {
		A[i] = 0.0;
	}

	for (k = 0; k < channels; k++) {
		printf("deconv: color channel %d...\n", k);
		g = image->map[k];
		for (x = 0; x < w1; x++) {
			border_x = x - a + w1; // w1 ������������ ��� ���������� ������ �������� %
			for (y = 0; y < h1; y++) { // ������ �� ������ ����� ����������� f
				border_y = y - b + h1; // h1 ������������ ��� ���������� ������ �������� %
				pix1 = x + y*w1;
				sum = 0;
				for (i = 0; i < w2; i++) {
					for (j = 0; j < h2; j++) { // ������ �� ������� ����� ��� g
						index_x = (border_x + i)%w1;
						index_y = (border_y + j)%h1;
						pix2 = index_x + index_y*w1;
						A[pix1*M + pix2] = h[j*w2 + i];
						sum += h[i + j*w2]*g[index_x + index_y*w1];
					}
				}
				printf("assign\n");
				A[pix1*M + N] = sum/div;
			}
		}

		// ������� ����, ������� ������� � A
		double diag_item, first_item, temp;

		// ������ ��� ������ ������
		for (j = 0; j < N; j++) { // ���� �� ��������
			// ����� ��������� �������
			for (i = j; i < N; i++) {
				diag_item = A[i*M + j];
				if (diag_item != 0) {
					break;
				}
			}
			if (diag_item == 0) {
				printf("deconv: system is incompatible\n");
			}
			// � �������� ������� ������
			if (i != j) {
				for (t = j; t < M; t++) {
					temp = A[i*M + t];
					A[i*M + t] = A[j*M + t];
					A[j*M + t] = temp;
				}
			}
			// ��������� ������ �� ������������ �������
			A[j*M + j] = 1.0;
			for (t = j + 1; t < M; t++) { // ���� �� ������
				A[j*M + t] /= diag_item;
			}

			// �������� ���������� ���� �������� ����������� ��������
			for (i = j + 1; i < N; i++) { // ���� �� �������
				first_item = A[i*M + j];
				A[i*M + j] = 0;
				for (t = j + 1; t < M; t++) { // ���� �� ��������� ������
					A[i*M + t] -= A[j*M + t]*first_item;
				}
			}
		}

		// �������� ��� ������ ������
		for (j = N - 1; j > 0; j--) { // ���� �� �������� � �������� �������
			for (i = j - 1; i >= 0; i--) { // ���� �� ������� � �������� �������
				A[i*M + N] -= A[j*M + N]*A[i*M + j];
			}
		}

		// ��������� �����������
		f = latent->map[k];
		for (i = 0; i < size1; i++) {
			f[i] = A[i*M + N];
		}
	}

	return latent;
}

/*
 * ��������� �� ����������� ������ ����������� �����
 */
COMPLEX_ARRAYS *_form_complex_array(IMAGE *image, int desirable_size = 0) {
	COMPLEX_ARRAYS *result;
	comp *mas;
	double *m;
	int new_size, size;
	int i, k, channels;

	channels = image->channels;
	size = image->height*image->width;
	if (desirable_size < size) {
		desirable_size = size;
	}
	result = new COMPLEX_ARRAYS();
	new_size = 1;
	while(new_size < desirable_size) {
		new_size *= 2;
	}

	result->size = new_size;
	for (k = 0; k < channels; k++) {
		m = image->map[k];
		mas = new comp[new_size];
		for (i = 0; i < size; i++) {
			mas[i].imag(0);
			mas[i].real(m[i]);
		}
		for (i = size; i < new_size; i++) {
			mas[i].imag(0);
			mas[i].real(0);
		}
		result->arrays[k] = mas;
	}

	return result;
}

/*
 * ��������� �������������� �����
 */
FOURIER_IMAGE *_FT(IMAGE *image) {
	FOURIER_IMAGE *fourier_image; // ����� �������� ����� �����-��������������
	comp *comp_map; // ����������� ���������� �����
	double *map; // ���������� ����� �����������
	double real, imag, value; // ������������� �������� � �����������
	int index1, index2; // ������� ��������� ����������� � ������
	int w, h; // ������� �����������
	int size; // ���������� �������� �� �����������
	int sign; // 1 ��� -1 ��� ���������� �� (-1)^(x+y) ��� �������������
	int channels; // ���������� �������� �������
	int x, y, u, v, k; // �������� ������

	w = image->width;
	h = image->height;
	size = w*h;
	channels = image->channels;
	fourier_image = new FOURIER_IMAGE();
	fourier_image->channels = channels;
	fourier_image->width = w;
	fourier_image->height = h;

	for (k = 0; k < channels; k++) {
		printf("channel: %d, ", k);
		map = image->map[k];
		comp_map = new comp[size];

		printf("to %d: ", w);
		for (u = 0; u < w; u++) {
			printf("u = %d\n", u);
			for (v = 0; v < h; v++) {
				printf("v = %d\n", v);
				index2 = v*w + u;
				real = 0;
				imag = 0;
				for (x = 0; x < w; x++) {
					for (y = 0; y < h; y++) {
						index1 = y*w + x;
						sign = 1 - 2*((x+y)%2); // �������� ����������� ���������� �� (-1)^(x+y)
						real += sign*map[index1]*cos(2*PI*(u*x/w+v*y/h)*(PI/180));
						imag += -sign*map[index1]*cos(2*PI*(u*x/w+v*y/h)*(PI/180));
					}
				}
				comp_map[index2].real(real/size);
				comp_map[index2].imag(imag/size);
			}
		}
		fourier_image->map[k] = comp_map;
		printf("\n");
	}
	printf("\n");

	return fourier_image;
}

/*
 * �������� �������������� �����
 */
IMAGE *_IFT(FOURIER_IMAGE *fourier_image) {
	IMAGE *image; // �����������
	comp *comp_map; // ����������� ���������� �����
	comp value; // ������������� �������� � �����������
	double *map; // ���������� ����� �����������
	double real;  // ������������� �������� � �����������
	int index1, index2; // ������� ��������� ����������� � ������
	int w, h; // ������� �����������
	int size; // ���������� �������� �� �����������
	int sign; // 1 ��� -1 ��� ���������� �� (-1)^(x+y) ��� �������������
	int channels; // ���������� �������� �������
	int x, y, u, v, k; // �������� ������

	w = fourier_image->width;
	h = fourier_image->height;
	size = w*h;
	channels = fourier_image->channels;
	image = new IMAGE();
	image->channels = channels;
	image->width = w;
	image->height = h;

	for (k = 0; k < channels; k++) {
		printf("channel: %d, ", k);
		comp_map = fourier_image->map[k];
		map = new double[size];

		printf("to %d: ", w);
		for (x = 0; x < w; x++) {
			printf("%d ", x);
			for (y = 0; y < h; y++) {
				index1 = y*w + x;
				real = 0;
				for (u = 0; u < w; u++) {
					for (v = 0; v < h; v++) {
						index2 = v*w + u;
						value.real(cos(2*PI*(u*x/w+v*y/h)*(PI/180)));
						value.imag(sin(2*PI*(u*x/w+v*y/h)*(PI/180)));
						real += (comp_map[index2]*value).real();
					}
				}
				sign = 1 - 2*((x+y)%2); // �������� ����������� ���������� �� (-1)^(x+y)
				map[index1] = sign*real;
			}
		}
		image->map[k] = map;
		printf("\n");
	}
	printf("\n");

	return image;
}

/*
 * ��������� ����������
 */
IMAGE *deconvinverse(IMAGE *image, IMAGE *psf) {
	int w1, h1, w2, h2; // ������� ����������� � ���
	int size1, size2; // ���������� �������� ����������� � ���
	int channels; // ���������� �������� ������� �����������
	int i, j, k; // �������� ������
	int a, b; // ���������� � ���������� ���
	IMAGE *latent; // ����������������� �����������
	double div; // ����������� ��� 
	double *h, *g; // ���������� �����

	w2 = psf->width;
	h2 = psf->height;

	if (psf->channels > 1) {
		printf("deconvlucy: PSF should be a grayscale image\n");
		return 0;
	}
	if (w2%2 != 1 || h2%2 != 1) {
		printf("deconvlucy: PSF cannot be of a size (%d, %d)\n", w2, h2);
		return 0;
	}

	a = w2/2; // �������������� ���������� �� ������ psf �� ����
	b = h2/2; // ������������ ���������� �� ������ psf �� ����
	channels = image->channels;
	w1 = image->width;
	h1 = image->height;
	size1 = w1*h1;
	size2 = w2*h2;

	latent = createImage(w1, h1, channels);
	//latent = copyImage(image);
	h = psf->map[0];

	// ���������� ����������� ���
	div = getPSFDivisor(psf);
	if (div == 0) {
		return 0;
	}

	COMPLEX_ARRAYS *complex_image, *complex_psf;
	int complex_size1, complex_size2;
	comp *complex_image_map, *complex_psf_map, complex_value;

	complex_image = _form_complex_array(image);
	complex_size1 = complex_image->size;
	complex_psf = _form_complex_array(psf, complex_size1);
	//complex_size2 = complex_psf->size;
	complex_psf_map = complex_psf->arrays[0];

	fourier_transform(complex_psf_map, complex_size1);
	for (k = 0; k < channels; k++) {
		complex_image_map = complex_image->arrays[k];
		fourier_transform(complex_image_map, complex_size1);
		for (i = 0; i < complex_size1; i++) {
			complex_value = complex_psf_map[i];
			if (complex_value.imag() != 0 || complex_value.real() != 0) {
				complex_image_map[i] /= complex_value;
			}
		}
	}
	for (k = 0; k < channels; k++) {
		complex_image_map = complex_image->arrays[k];
		inverse_fourier_transform(complex_image_map, complex_size1);
		for (i = 0; i < size1; i++) {
			latent->map[k][i] = complex_image_map[i].real();
		}
	}

	return latent;
}

/*
 * �������� ����-����������
 */
IMAGE *deconvlucy(IMAGE *image, IMAGE *psf, int iterations) {
	int w1, h1, w2, h2; // ������� ����������� � ���
	int size1, size2; // ���������� �������� ����������� � ���
	int channels; // ���������� �������� ������� �����������
	int i, j, k; // �������� ������
	int a, b; // ���������� � ���������� ���
	IMAGE *latent; // ����������������� �����������
	IMAGE *psf_inv; // ���������� ���, �� ���� psf(-x, -y)
	IMAGE *temp1, *temp2; // ���������� ��� �������� ������������� �����������
	double div; // ����������� ��� 
	double *h, *h_inv, *g, *t; // ���������� �����

	w2 = psf->width;
	h2 = psf->height;

	if (psf->channels > 1) {
		printf("deconvlucy: PSF should be a grayscale image\n");
		return 0;
	}
	if (w2%2 != 1 || h2%2 != 1) {
		printf("deconvlucy: PSF cannot be of a size (%d, %d)\n", w2, h2);
		return 0;
	}

	a = w2/2; // �������������� ���������� �� ������ psf �� ����
	b = h2/2; // ������������ ���������� �� ������ psf �� ����
	channels = image->channels;
	w1 = image->width;
	h1 = image->height;
	size1 = w1*h1;
	size2 = w2*h2;

	latent = copyImage(image);
	temp1 = createImage(w1, h1, channels);
	temp2 = createImage(w1, h1, channels);
	psf_inv = createImage(w2, h2, 1);

	h = psf->map[0];
	h_inv = psf_inv->map[0];
	for (i = 0; i < size2; i++) {
		h_inv[i] = h[size2 - i - 1];
	}

	// ���������� ����������� ���
	div = getPSFDivisor(psf);
	if (div == 0) {
		return 0;
	}

	for (k = 0; k < iterations; k++) {
		printf("*%d", k);
		_conv(latent->map, h, channels, w1, h1, w2, h2, a, b, div, temp1->map);
		for (j = 0; j < channels; j++) {
			t = temp1->map[j];
			g = image->map[j];
			for (i = 0; i < size1; i++) {
				t[i] = g[i]/t[i];
			}
		}
		_conv(temp1->map, h_inv, channels, w1, h1, w2, h2, a, b, div, temp2->map);
		for (j = 0; j < channels; j++) {
			g = latent->map[j];
			t = temp2->map[j];
			for (i = 0; i < size1; i++) {
				g[i] = g[i]*t[i];
			}
		}
	}
	printf("\n");
	deleteImage(temp1);
	deleteImage(temp2);
	deleteImage(psf_inv);
	return latent;
}

/*
 * ����������� ����������
 */
IMAGE *superresolution(IMAGE *image) {
	IMAGE *big_image; // �������� �����������
	int w, h; // ������� �������� �����������
	int channels; // ���������� �������� ������� ����������
	int i, j, k; // �������� ������		
	double *map; // ���������� ����� �������� �����������
	double *big_map; // ���������� ����� ��������� �����������
	double value; // ������� ������� �������� �����������

	w = image->width;
	h = image->height;
	channels = image->channels;
	big_image = createImage(w*2, h*2, channels);

	for (k = 0; k < channels; k++) {
		map = image->map[k];
		big_map = big_image->map[k];
		//big_map[0] = 1.0;
		for (i = 0; i < w; i++) {
			for (j = 0; j < h; j++) {
				//big_map[i*h*2 + j] = map[(i/2)*h + j/2];
				value = map[j*w + i];
				big_map[(2*j)*(w*2) + i*2] = value;
				big_map[(2*j)*(w*2) + i*2 + 1] = (value + map[j*w + (i+1)%w])/2;
				big_map[(2*j)*(w*2) + w*2 + i*2] = (value + map[((j+1)%h)*w + i])/2;
				big_map[(2*j)*(w*2) + w*2 + i*2 + 1] = (value + map[((j+1)%h)*w + (i+1)%w])/2;
			}
		}
	}
	big_map = big_image->map[k];
	return big_image;
}

/*
 * �������� ������ �� ������� ��������� �������
 */
void normalize(IMAGE *image) {
	int size; // ���������� �������� �����������
	int channels; // ���������� �������� ������� �����������
	int i, j; // �������� ������
	double *map; // ���������� ����� �����������

	if (image == 0) {
		printf("normalize: cannot normalize the image, becase it's 0\n");
		return;
	}
	size = image->width * image->height;
	channels = image->channels;
	for (i = 0; i < channels; i++) {
		map = image->map[i];
		for (j = 0; j < size; j++) {
			if (map[j] > 1.0) map[j] = 1.0;
			if (map[j] < 0.0) map[j] = 0.0;
		}
	}
}

/*
 * Main
 */
int main()
{
	IMAGE *image, *grayscale_image, *res_image;
	IMAGE *blured;
	IMAGE *psf;
	BYTE *map;
	//image = generateImage(130, 100, 3);
	image = loadImage("images/no_noise.png", PNG);
	//image = loadImage("images/naive.png", PNG);
	//image = loadImage("images/houses/rastr4.bmp", BMP);
	//psf = generatePSF(3, 3, PSF_RADIAL);
	//psf = generatePSF(3, 3, PSF_LINEAR);
	//saveImage(psf, "images/psf.png", PNG);
	//printf("div = %f\n", getPSFDivisor(psf));
	//psf = loadImage("psf/psf13x13_detected.png", PNG);
	psf = loadImage("psf/psf19x19_motion.png", PNG);
	//psf = loadImage("psf/psf5x5_blur.png", PNG);
	//psf = loadImage("psf/psf5x5_identity.png", PNG);
	grayscale(psf);

	//grayscale(image);
	//image = superresolution(superresolution(image));
	//inverse(image);
	//inverse(image);
	//laplace(image, FOUR_SIDES);

	image = _IFT(_FT(image));
	//image = conv(image, psf);
	//image = deconvinverse(image, psf);
	//laplace(image, FOUR_SIDES);
		
	//image = deconvlucy(image, psf, 10);
	normalize(image);
	
	//saveImage(image, "images/naive_conv_deconv.png", PNG);
	saveImage(image, "images/no_noise_res.png", PNG);
	return 0;
}

