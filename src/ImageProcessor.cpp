/*******************************************************************************
* ImageProcessor.cpp
*
*	Contains methods for handling the decoding of 2d barcodes from images. Mostly
*	a functionality wrapper around libdmtx which
******************************************************************************/

#include "UaDebug.h"
#include "ImageProcessor.h"

#include <string.h>
#include <iostream>
using namespace std;

#ifdef _VISUALC_
// disable fopen warnings
#pragma warning(disable : 4996)
#endif

/*
*	decodeDmtxImage
*	@params - image: pointer to the DmtxImage to decode
*	@return - none
*
*	As of right now, this function takes a DmtxImage as the parameter,
*	and decodes a single 2d barcode in the Image.
*
*	TODO: return the message instead of printing it out.
*	TODO: Improve decoding (decode more than one barcode)
*/
void decodeDmtxImage(DmtxImage* image){
	DmtxDecode     *dec;
	DmtxRegion     *reg;
	DmtxMessage    *msg;
	int totalBytes, headerBytes;
	unsigned char *pnm;

	if (image == NULL) {
		UA_ERROR(" could not create DMTX image");
	}

	UA_DOUT(1, 1, "image width: " << dmtxImageGetProp(image, DmtxPropWidth));
	UA_DOUT(1, 1, "image height: " << dmtxImageGetProp(image, DmtxPropHeight));
	UA_DOUT(1, 1, "image bits per pixel: "
		<< dmtxImageGetProp(image, DmtxPropBitsPerPixel));
	UA_DOUT(1, 1, "image row size bytes: "
		<< dmtxImageGetProp(image, DmtxPropRowSizeBytes));

	dec = dmtxDecodeCreate(image, 1);
	assert(dec != NULL);

	UA_DEBUG(
		// save image to a PNM file
		FILE * fh;
		pnm = dmtxDecodeCreateDiagnostic(dec, &totalBytes, &headerBytes, 0);
		fh = fopen("out.pnm", "w");
		fwrite(pnm, sizeof(unsigned char), totalBytes, fh);
		fclose(fh);
	);

	dmtxDecodeSetProp(dec, DmtxPropScanGap, DmtxPropScanGap);
	dmtxDecodeSetProp(dec, DmtxPropSquareDevn, DmtxPropSquareDevn);
	dmtxDecodeSetProp(dec, DmtxPropEdgeThresh, DmtxPropEdgeThresh);

	reg = dmtxRegionFindNext(dec, NULL);

	int read = 0;
	int total = 0;
	if (reg != NULL) UA_DOUT(1, 1, "found a region...");
	while (reg != NULL) {

		msg = dmtxDecodeMatrixRegion(dec, reg, DmtxUndefined);
		if(msg != NULL) {
			char buf[1024];
			memcpy(buf, msg->output, msg->outputIdx);
			buf[msg->outputIdx] = 0;
			cout << "barcode: \"" << buf << "\"" << endl;
			dmtxMessageDestroy(&msg);
			read++;
		}
		else{
			UA_DOUT(1, 1, "Unable to read region");
		}
		total++;
		dmtxRegionDestroy(&reg);
		reg = dmtxRegionFindNext(dec, NULL);
	}
	UA_DOUT(1, 1, "Read " << total << " regions, decoded " << read << " of them\n");
	dmtxDecodeDestroy(&dec);
}

/*
*	decodeDib
*	@params - filename: char* corresponding to the filename of an image
*	@return - none
*
*	Create a file from the filename given, then create a DmtxImage from this
*	file. If a DmxtImage can be created, decode it.
*
*	TODO: return the decoded string.
*/
void decodeDib(char * filename) {
	Dib dib;
	DmtxImage *image;

	UA_ASSERT(filename != NULL);

	dib = dibAllocate();
	UA_ASSERT(dib != NULL);
	image = createDmtxImageFromFile(filename, dib);

	if (image == NULL) {
		UA_ERROR(" could not create DMTX image");
	}

	decodeDmtxImage(image);
	dmtxImageDestroy(&image);

	dibDestroy(dib);
}

/*
*	createDmtxImageFromFile
*	@params - filename: char* corresponding to the filename of an image
*			  dib - a blank Divice Independant Bitmap to read the image into
*	@return - DmtxImage: the newly created image from the file.
*
*	Open the file and create a DmtxImage out of it.
*/
DmtxImage * createDmtxImageFromFile(char* filename, Dib dib){
	FILE * fh;
	DmtxImage * image;

	UA_ASSERT(filename != NULL);
	UA_ASSERT(dib != NULL);

	fh = fopen(filename, "r"); // C4996
	UA_ASSERT(fh != NULL);
	readDibHeader(fh, dib);
	readDibPixels(fh, dib);
	fclose(fh);

	// create dmtxImage from the dib
	image = dmtxImageCreate(dibGetPixelBuffer(dib), dibGetWidth(dib), dibGetHeight(dib),
		DmtxPack24bppRGB);

	//set the properties (pad bytes, flip)
	dmtxImageSetProp(image, DmtxPropRowPadBytes, dibGetRowPadBytes(dib));
	dmtxImageSetProp(image, DmtxPropImageFlip, DmtxFlipY); // DIBs are flipped in Y

	return image;
}

/*
*	rotateImage
*
*	@param - src: DmtxImage to be rotated 90 degrees
*	@return - a new image of src rotated 90 degrees
*
*	rotateImage does not rotate the original Image, instead it returns a new
*	image. Images are rotated 90 degrees.
*
*	TODO: returning a new image might not be the right thing to do here, but
*			it makes the most sense right now.
*/
DmtxImage* rotateImage(DmtxImage* src)
{
	DmtxImage *dest;
	// Get the info from the src image
	int width = src->width;
	int height = src->height;
	int rowPadBytes = src->rowPadBytes;
	int channelCount = src->channelCount;
	int bytesPerPixel = src->bytesPerPixel;
	//Need to allocate a new unsigned char* for pixels, since we dont want to modify
	//the pixel array of src
	int arraySize = width * height * bytesPerPixel;//i think this is right
	unsigned char* pBits = (unsigned char *)malloc(arraySize);
	dest = dmtxImageCreate(pBits, height, width, DmtxPack24bppRGB);
	//used to figure out where to get the pixel from
	int srcOffset = 0;
	int srcCol = 0;
	int srcRow = 0;
	int value = 0;
	
	//loop through the pixels and copy pixel by pixel. slow but i dont know a faster
	//method right now.
	for(int col=0; col<width; col++){ 
		for(int row=0; row<height; row++){
			for(int channel=0; channel < channelCount; channel++){
				srcOffset = (height - 1 - col)*width + row;
				srcCol = srcOffset % width;
				srcRow = srcOffset / width;
				dmtxImageGetPixelValue(src, srcCol, srcRow, channel, &value);
				dmtxImageSetPixelValue(dest, row, col, channel, value);
			}
		}
	}
	//want to set the properties the same as src
	dmtxImageSetProp(dest, DmtxPropRowPadBytes, rowPadBytes);
	dmtxImageSetProp(dest, DmtxPropImageFlip, DmtxFlipY); // DIBs are flipped in Y
	return dest;
}
