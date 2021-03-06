/*
 * TestDmScanLib.cpp
 *
 *  Created on: 2012-11-04
 *      Author: nelson
 */

#define _CRT_SECURE_NO_DEPRECATE

#include "DmScanLib.h"
#include "Image.h"
#include "decoder/Decoder.h"
#include "decoder/DecodeOptions.h"
#include "decoder/WellDecoder.h"
#include "test/TestCommon.h"
#include "test/ImageInfo.h"
#include "decoder/DmtxDecodeHelper.h"

#include <dmtx.h>

#include <algorithm>
#include <opencv/cv.h>

#include <stdexcept>
#include <stddef.h>
#include <sys/types.h>
#include <sstream>
#include <iostream>
#include <fstream>

#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>
#include <gtest/gtest.h>

using namespace dmscanlib;

namespace {

TEST(TestDmScanLib, invalidRects) {
	FLAGS_v = 0;

    std::unique_ptr<DecodeOptions> decodeOptions = test::getDefaultDecodeOptions();
    std::vector<std::unique_ptr<const WellRectangle> > wellRects;
	DmScanLib dmScanLib(1);
    int result = dmScanLib.decodeImageWells("testImages/8x12/96tubes.bmp", *decodeOptions,
            wellRects);
	EXPECT_EQ(SC_INVALID_NOTHING_DECODED, result);
}

TEST(TestDmScanLib, invalidImage) {
	FLAGS_v = 0;

	std::unique_ptr<const WellRectangle> wrect(new WellRectangle("label", 0,0,10,10));

	std::vector<std::unique_ptr<const WellRectangle> > wellRects;
    wellRects.push_back(std::move(wrect));

    std::unique_ptr<DecodeOptions> decodeOptions = test::getDefaultDecodeOptions();
	DmScanLib dmScanLib(1);
	int result = dmScanLib.decodeImageWells("xyz.bmp", *decodeOptions, wellRects);
	EXPECT_EQ(SC_INVALID_IMAGE, result);
}

TEST(TestDmScanLib, decodeImage) {
    FLAGS_v = 5;

    std::string fname("testImages/8x12/96tubes.bmp");
    //std::string fname("testImages/12x12/stanford_12x12_1.jpeg");

	DmScanLib dmScanLib(1);
    int result = test::decodeImage(fname, dmScanLib, 8, 12);

	EXPECT_EQ(SC_SUCCESS, result);
	EXPECT_TRUE(dmScanLib.getDecodedWellCount() > 0);

	if (dmScanLib.getDecodedWellCount() > 0) {
		VLOG(1) << "number of wells decoded: " << dmScanLib.getDecodedWells().size();
	}
}

void writeAllDecodeResults(std::vector<std::string> & testResults, bool append = false) {
    std::ofstream ofile;
    if (append) {
        ofile.open("all_images_results.csv", std::ios::app);
    } else {
        ofile.open("all_images_results.csv");
    }

	for (unsigned i = 0, n = testResults.size(); i < n; ++i) {
		ofile <<  testResults[i] << std::endl;
	}
	ofile.close();
}

// check that the decoded message matches the one in the "nfo" file
void checkDecodeInfo(DmScanLib & dmScanLib, dmscanlib::test::ImageInfo & imageInfo) {
    const std::map<std::string, const WellDecoder *> & decodedWells = dmScanLib.getDecodedWells();
    for (std::map<std::string, const WellDecoder *>::const_iterator ii = decodedWells.begin();
            ii != decodedWells.end(); ++ii) {
        const WellDecoder & decodedWell = *(ii->second);
        const std::string & label = decodedWell.getLabel();
        const std::string * nfoDecodedMsg = imageInfo.getBarcodeMsg(label);

        if ((decodedWell.getMessage().length() > 0) && (nfoDecodedMsg != NULL)) {
            EXPECT_EQ(*nfoDecodedMsg, decodedWell.getMessage()) << "label: " << label;
        }
    }
}

class DecodeTestResult {
public:
    bool infoFileValid;
	int decodeResult;
    unsigned totalTubes;
    unsigned totalDecoded;
    double decodeTime;

    DecodeTestResult() {
    }

};

std::unique_ptr<DecodeTestResult> decodeFromInfo(
        std::string & infoFilename,
        DecodeOptions & decodeOptions,
        DmScanLib & dmScanLib) {

    std::unique_ptr<DecodeTestResult> testResult(new DecodeTestResult());

    dmscanlib::test::ImageInfo imageInfo(infoFilename);
    testResult->infoFileValid = imageInfo.isValid();

    if (testResult->infoFileValid) {
        testResult->totalTubes = imageInfo.getDecodedWellCount();
        std::vector<std::unique_ptr<const WellRectangle> > wellRects;
        test::getWellRectsForBoundingBox(
                imageInfo.getBoundingBox(),
                imageInfo.getPalletRows(),
                imageInfo.getPalletCols(),
                imageInfo.getOrientation(),
                imageInfo.getBarcodePosition(),
                wellRects);

        util::DmTime start;
        testResult->decodeResult = dmScanLib.decodeImageWells(
                imageInfo.getImageFilename().c_str(),
                decodeOptions,
                wellRects);
        util::DmTime end;

        testResult->decodeTime = end.difftime(start)->getTime();

        if (testResult->decodeResult == SC_SUCCESS) {
            testResult->totalDecoded = dmScanLib.getDecodedWellCount();
            checkDecodeInfo(dmScanLib, imageInfo);
        }
    }

    return testResult;

}

TEST(TestDmScanLib, readInfoFiles) {
    FLAGS_v = 1;

    std::string dirname("testImageInfo");
    std::vector<std::string> filenames;
    bool result = test::getTestImageInfoFilenames(dirname, filenames);
    EXPECT_EQ(true, result);

    std::stringstream ss;
    std::vector<std::string> testResults;
    testResults.push_back("#filename,decoded,total,ratio,time (sec)");

    for (unsigned i = 0, n = filenames.size(); i < n; ++i) {
        ss.str("");
        VLOG(1) << "test image info: " << filenames[i];

        dmscanlib::test::ImageInfo imageInfo(filenames[i]);
        bool result = imageInfo.isValid();
        EXPECT_EQ(true, result);
    }

}

TEST(TestDmScanLib, decodeFromInfo) {
    FLAGS_v = 3;

    //std::string infoFilename("testImageInfo/8x12/calgary2.nfo");
    //std::string infoFilename("testImageInfo/8x12/new_tubes.nfo");
    std::string infoFilename("testImageInfo/9x9/stanford_9x9_1.nfo");

    std::unique_ptr<DecodeOptions> defaultDecodeOptions = test::getDefaultDecodeOptions();
    DecodeOptions decodeOptions(
            0.2,
            0.4,
            0.0,
            defaultDecodeOptions->squareDev,
            defaultDecodeOptions->edgeThresh,
            defaultDecodeOptions->corrections,
            defaultDecodeOptions->shrink);

    DmScanLib dmScanLib(0);
    std::unique_ptr<DecodeTestResult> testResult =
            decodeFromInfo(infoFilename, decodeOptions, dmScanLib);

    EXPECT_TRUE(testResult->infoFileValid);
    EXPECT_EQ(SC_SUCCESS, testResult->decodeResult);
    if (testResult->decodeResult == SC_SUCCESS) {
        EXPECT_TRUE(testResult->totalDecoded > 0);

        VLOG(1) << "total: " << testResult->totalTubes
                << ", decoded: " << testResult->totalDecoded
                << ", time taken: " << testResult->decodeTime;

    }
}

//TEST(TestDmScanLib, DISABLED_decodeAllImages) {
TEST(TestDmScanLib, decodeAllImages) {
    FLAGS_v = 1;

    std::string dirname("testImageInfo");
    std::vector<std::string> filenames;
    bool result = test::getTestImageInfoFilenames(dirname, filenames);
    EXPECT_EQ(true, result);

    std::vector<std::string> testResults;
    testResults.push_back("#filename,decoded,total,ratio,time (sec)");

    unsigned totalTubes = 0;
    unsigned totalDecoded = 0;
    double totalTime = 0;
    std::stringstream ss;
    std::unique_ptr<DecodeOptions> defaultDecodeOptions = test::getDefaultDecodeOptions();
    DecodeOptions decodeOptions(
            0.2,
            0.4,
            0.15,
            defaultDecodeOptions->squareDev,
            defaultDecodeOptions->edgeThresh,
            defaultDecodeOptions->corrections,
            defaultDecodeOptions->shrink);

    for (unsigned i = 0, n = filenames.size(); i < n; ++i) {
        ss.str("");
        VLOG(1) << "test image info: " << filenames[i];

        DmScanLib dmScanLib(0);
        std::unique_ptr<DecodeTestResult> testResult =
                decodeFromInfo(filenames[i], decodeOptions, dmScanLib);
        EXPECT_TRUE(testResult->infoFileValid);
        EXPECT_EQ(SC_SUCCESS, testResult->decodeResult);

        ss << filenames[i];

        if (testResult->decodeResult == SC_SUCCESS) {
            EXPECT_TRUE(testResult->totalDecoded > 0);

            ss << "," << testResult->totalDecoded << ","
                    << testResult->totalTubes << ","
                    << testResult->totalDecoded / static_cast<float>(testResult->totalTubes) << ","
                    << testResult->decodeTime;

            VLOG(1) << "decoded: " << testResult->totalDecoded
                    << ", total: " << testResult->totalTubes
                    << ", time taken: " << testResult->decodeTime;

            totalDecoded += testResult->totalDecoded;
            totalTubes += testResult->totalTubes;
            totalTime += testResult->decodeTime;
        }
		testResults.push_back(ss.str());
	}

    ss.str("");
    ss << "decoded:," << totalDecoded << ", total:," << totalTubes;
    testResults.push_back(ss.str());
    ss.str("");

    double avgDecodeTime = totalTime / filenames.size();
    ss << "average decode time:," << avgDecodeTime;
    testResults.push_back(ss.str());

    VLOG(1) << "total tubes: " << totalTubes
            << ", total decoded: " << totalDecoded
            << ", average decode time: " << avgDecodeTime;

    writeAllDecodeResults(testResults);
}

//TEST(TestDmScanLib, DISABLED_decodeAllImagesAllParameters) {
TEST(TestDmScanLib, decodeAllImagesAllParameters) {
    FLAGS_v = 1;

    std::string dirname("testImageInfo");
    std::vector<std::string> filenames;
    bool result = test::getTestImageInfoFilenames(dirname, filenames);
    EXPECT_EQ(true, result);

    std::unique_ptr<DecodeOptions> defaultDecodeOptions = test::getDefaultDecodeOptions();
    std::vector<std::string> testResults;
    testResults.push_back("#filename,decoded,total,ratio,time (sec),minEdgeFactor,maxEdgeFactor,scanGapFactor");
    writeAllDecodeResults(testResults, false); // clear previous contents of the file

    std::stringstream ss;

    for (double maxEdge = 0.15; maxEdge <= 1.05; maxEdge += 0.05) {
       for (double minEdge = 0, n = maxEdge + 0.05; minEdge < n; minEdge += 0.05) {
    		for (double scanGap = 0; scanGap <= 1.05; scanGap += 0.05) {
    			unsigned totalTubes = 0;
    			unsigned totalDecoded = 0;
    			double totalTime = 0;

    			testResults.clear();

    			DecodeOptions decodeOptions(
    					minEdge,
    					maxEdge,
    					scanGap,
                        defaultDecodeOptions->squareDev,
                        defaultDecodeOptions->edgeThresh,
                        defaultDecodeOptions->corrections,
                        defaultDecodeOptions->shrink);

                for (unsigned i = 0, n = filenames.size(); i < n; ++i) {
                    ss.str("");
                    VLOG(1) << "test image info: " << filenames[i];

                    DmScanLib dmScanLib(0);
                    std::unique_ptr<DecodeTestResult> testResult =
                            decodeFromInfo(filenames[i], decodeOptions, dmScanLib);
                    EXPECT_TRUE(testResult->infoFileValid);
                    EXPECT_EQ(SC_SUCCESS, testResult->decodeResult);

                    ss << filenames[i];

                    if (testResult->decodeResult == SC_SUCCESS) {
                        EXPECT_TRUE(testResult->totalDecoded > 0);

                        ss << "," << testResult->totalDecoded
                                << "," << testResult->totalTubes
                                << "," << testResult->totalDecoded / static_cast<float>(testResult->totalTubes)
                                << "," << testResult->decodeTime
                                << "," << minEdge
                                << "," << maxEdge
                                << "," << scanGap;

                                VLOG(1) << "decoded: " << testResult->totalDecoded
                                << ", total: " << testResult->totalTubes
                                << ", time taken: " << testResult->decodeTime
                                << ", minEdgeFactor: " << minEdge
                                << ", maxedgeFactor: " << maxEdge
                                << ", scanGapFactor: " << scanGap;

                        totalDecoded += testResult->totalDecoded;
                        totalTubes += testResult->totalTubes;
                        totalTime += testResult->decodeTime;
                    }

                    testResults.push_back(ss.str());
                }

                double avgDecodeTime = totalTime / filenames.size();
                ss.str("");
                ss << "RESULTS:,decoded:," << totalDecoded
                        << ", total:," << totalTubes
                        << ", average decode time:," << avgDecodeTime
                        << ", minEdgeFactor:," << minEdge
                        << ", maxedgeFactor:," << maxEdge
                        << ", scanGapFactor:," << scanGap;
                testResults.push_back(ss.str());
                writeAllDecodeResults(testResults, true);
            }
        }
    }
}

} /* namespace */
