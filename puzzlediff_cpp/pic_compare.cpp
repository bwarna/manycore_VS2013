
extern "C"
{
#include "puzzle_common.h"
#include "puzzle.h"
}

#include <stdio.h>
#include <stdlib.h>
#include "pgetopt.hpp"
#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <ctime>
#ifndef __cilk
#include <cilk/cilk_stub.h>
#endif
#include <cilk\cilk.h>
#include "listdir.hpp"

#define CLOSE_RESEMBLANCE_THRESHOLD 0.12

using namespace std;

struct img_distance {
	string name;
	double distance;
};

inline bool operator<(const img_distance& lhs, const img_distance& rhs)
{
	return lhs.distance < rhs.distance;
}

typedef struct Opts_ {
	const char *reference_pic;
	const char *folderPath;
	bool printToFile;
	const char *outFile;
} Opts;


void usage(void)
{
	puts("\nUsage: puzzle-diff [-o <output filename>] <file> <directory>\n\n"
		"Visually compares images in the directory and returns\n"
		"the images that are closest to the reference image.\n\n");
	exit(EXIT_SUCCESS);
}


int parse_opts(Opts * const opts,
	int argc, char **argv) {
	int opt;
	extern char *poptarg;
	extern int poptind;

	opts->printToFile = false;

	while ((opt = pgetopt(argc, argv, "o:")) != -1) {
		switch (opt) {
		case 'o':
			opts->printToFile = true;
			opts->outFile = poptarg;
			break;
		default:
			break;
		}
	}
	argc -= poptind;
	argv += poptind;
	if (argc != 2) {
		usage();
	}
	opts->reference_pic = *argv++;
	opts->folderPath = *argv;

	return 0;
}

int main(int argc, char *argv[])
{
	clock_t begin = clock();

	Opts opts;
	parse_opts(&opts, argc, argv);

	PuzzleContext context;
	PuzzleCvec cvec_reference;
	char comparison_pic[256];

	int fix_for_texts = 1;

	// Init vectors
	puzzle_init_context(&context);
	puzzle_init_cvec(&context, &cvec_reference);

	// Load reference picture
	if (puzzle_fill_cvec_from_file(&context, &cvec_reference, opts.reference_pic) != 0) {
		fprintf(stderr, "Unable to read [%s]\n", opts.reference_pic);
		return 1;
	}

	img_distance current_image;


	vector<string> fileNamesVector;
	listDir(opts.folderPath, fileNamesVector);
	int files = fileNamesVector.size();


	vector<img_distance> vDistances(files);

	cout << "Checking " << files << " files." << endl;


	// Loop through files in folder.
	//cilk_for(vector<string>::iterator fileIter = fileNamesVector.begin(); fileIter != fileNamesVector.end(); ++fileIter) {
	cilk_for(int i = 0; i < files; i++) {
		//strcpy(comparison_pic, (fileNamesVector[i]).c_str());
		PuzzleCvec cvec_compare;
		puzzle_init_cvec(&context, &cvec_compare);
		// Create vector
		int puzzle_vec = puzzle_fill_cvec_from_file(&context, &cvec_compare, (fileNamesVector[i]).c_str());
		if (puzzle_vec != 0) {
			fprintf(stderr, "Unable to read [%s]\n %d\n", (fileNamesVector[i]).c_str(), puzzle_vec);
			exit(1);
		}

		current_image.name = fileNamesVector[i];
		current_image.distance = puzzle_vector_normalized_distance(&context, &cvec_reference, &cvec_compare, fix_for_texts);

		vDistances[i] = current_image;
		/*if (current_image.distance <= CLOSE_RESEMBLANCE_THRESHOLD) {
		// Identical or close resemblancetopSimiliar.insert(current_image);
		vTopIdentical[i] = current_image;
		} else  {
		// Add to top ten similar list
		vTopSimilar[i] = current_image;
		}*/


		puzzle_free_cvec(&context, &cvec_compare);
	}
	cilk_sync;
	puzzle_free_cvec(&context, &cvec_reference);
	puzzle_free_context(&context);

	clock_t end = clock();
	double eltime = double(end - begin) / CLOCKS_PER_SEC;


	set<img_distance> topSimilar;
	set<img_distance> topIdentical;
	set<img_distance>::iterator setiter;
	img_distance curr;
	for (int i = 0; i < files; i++) {
		curr = vDistances[i];
		if (curr.distance <= CLOSE_RESEMBLANCE_THRESHOLD) {
			topSimilar.insert(curr);
			if (topSimilar.size() > 10) {
				// More than 10, remove last.
				setiter = topSimilar.end();
				--setiter;
				topSimilar.erase(setiter);
			}
		}
		else {
			topIdentical.insert(curr);
			if (topIdentical.size() > 10) {
				// More than 10, remove last.
				setiter = topIdentical.end();
				--setiter;
				topIdentical.erase(setiter);
			}

		}
	}
	/*for(viter=vTopSimilar.begin(); viter!=vTopSimilar.end();++viter) {
	topSimilar.insert(*viter);
	if (topSimilar.size() > 10) {
	// More than 10, remove last.
	setiter = topSimilar.end();
	--setiter;
	topSimilar.erase(setiter);
	}
	}
	for(viter=vTopIdentical.begin(); viter!=vTopIdentical.end();++viter) {
	topIdentical.insert(*viter);
	if (topIdentical.size() > 10) {
	// More than 10, remove last.
	setiter = topIdentical.end();
	--setiter;
	topIdentical.erase(setiter);
	}
	}*/


	if (opts.printToFile) {
		// Print to file
		ofstream outf;
		outf.open(opts.outFile, ofstream::out);
		outf.precision(3);

		outf << endl << " *** Pictures found to be similar to " << opts.reference_pic << " *** " << endl << endl;
		for (setiter = topSimilar.begin(); setiter != topSimilar.end(); ++setiter) {
			outf << setiter->distance << " " << setiter->name << endl;
		}

		outf << endl << endl << " *** Pictures found to be identical/close resemblance to " << opts.reference_pic << " *** " << endl << endl;
		for (setiter = topIdentical.begin(); setiter != topIdentical.end(); ++setiter) {
			outf << setiter->distance << " " << setiter->name << endl;
		}
		outf << endl;
		outf << eltime << endl;

		outf.close();

	}
	else {
		// Print in command line
		cout.precision(3);

		cout << endl << " *** Pictures found to be similar to " << opts.reference_pic << " *** " << endl << endl;
		for (setiter = topSimilar.begin(); setiter != topSimilar.end(); ++setiter) {
			cout << setiter->distance << " " << setiter->name << endl;
		}

		cout << endl << endl << " *** Pictures found to be identical/close resemblance to " << opts.reference_pic << " *** " << endl << endl;
		for (setiter = topIdentical.begin(); setiter != topIdentical.end(); ++setiter) {
			cout << setiter->distance << " " << setiter->name << endl;
		}
		cout << endl;


		cout << eltime;
	}

	return 0;

}