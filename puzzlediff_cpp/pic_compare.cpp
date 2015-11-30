
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
#include <dirent.h>
#include <set>
#include <string>
#include <ctime>
#ifndef __cilk
#include <cilk/cilk_stub.h>
#endif
#include <cilk\cilk.h>

#define CLOSE_RESEMBLANCE_THRESHOLD 0.12

using namespace std;

struct img_distance {
	string name;
	double distance;
};

typedef struct Opts_ {
    const char *reference_pic;    
    const char *folderPath;
    bool printToFile;
    const char *outFile;
} Opts;

inline bool operator<(const img_distance& lhs, const img_distance& rhs)
{
  return lhs.distance < rhs.distance;
}

void usage(void)
{
    puts("\nUsage: puzzle-diff [-o <output filename>] <file> <directory>\n\n"
         "Visually compares images in the directory and returns\n"
         "the images that are closest to the reference image.\n\n");
    exit(EXIT_SUCCESS);
}

bool isImage(char const *name)
{
    size_t len = strlen(name);
    // check that file extension is jpg, png or gif.
    return (len > 4 && (strcmp(name + len - 4, ".jpg") == 0 || strcmp(name + len - 4, ".png") == 0 || strcmp(name + len - 4, ".gif") == 0 ));
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
    PuzzleCvec cvec_reference, cvec_compare;
    char comparison_pic[256];

    int fix_for_texts = 1;

    // Init vectors
	puzzle_init_context(&context); 
    puzzle_init_cvec(&context, &cvec_reference);
    puzzle_init_cvec(&context, &cvec_compare);

    // Load reference picture
    if (puzzle_fill_cvec_from_file(&context, &cvec_reference, opts.reference_pic) != 0) {    
        fprintf(stderr, "Unable to read [%s]\n", opts.reference_pic);
        return 1;
    }
	cout << "testing";
    DIR *dir;
	struct dirent *fileEnt;
	
	set<img_distance> topSimiliar;
	set<img_distance> topIdentical;
    set<img_distance>::iterator iter;
	img_distance current_image;

	if ((dir = opendir (opts.folderPath)) != NULL) {
		
		// Loop through files in folder.
		while ((fileEnt = readdir (dir)) != NULL) {
			if (isImage(fileEnt->d_name)) {
				// Combine folder path with the image name
				strcpy(comparison_pic, opts.folderPath);
				strcat(comparison_pic, fileEnt->d_name);

				// Create vector
				int puzzle_vec =  puzzle_fill_cvec_from_file(&context, &cvec_compare, comparison_pic); //cilk up this call - so far only crashes
				if (puzzle_vec != 0) {
					fprintf(stderr, "Unable to read [%s]\n", comparison_pic);
					return 1;
				}
				//old create vector
			    /*if (puzzle_fill_cvec_from_file(&context, &cvec_compare, comparison_pic) != 0) {
			        fprintf(stderr, "Unable to read [%s]\n", comparison_pic);
			        return 1;
			    }*/

			    current_image.name = fileEnt->d_name;
				current_image.distance = puzzle_vector_normalized_distance(&context, &cvec_reference, &cvec_compare, fix_for_texts);
				
				
	    		//cout << fileEnt->d_name << " dist: " << distance << "\n";

			    if (current_image.distance <= CLOSE_RESEMBLANCE_THRESHOLD) {
			    	// Identical or close resemblancetopSimiliar.insert(current_image);
		    		topIdentical.insert(current_image);
			    	if (topIdentical.size() > 10) {
			    		// More than 10, remove last.
			    		iter = topIdentical.end();
			    		--iter;
			    		topIdentical.erase(iter);
			    	}
			    } else  {
			    	// Add to top ten similar list
		    		topSimiliar.insert(current_image);
			    	if (topSimiliar.size() > 10) {
			    		// More than 10, remove last.
			    		iter = topSimiliar.end();
			    		--iter;
			    		topSimiliar.erase(iter);
			    	}
			    }
				
			}
		}
		closedir (dir);
	} else {
		perror ("Could not open directory");
	}

    puzzle_free_cvec(&context, &cvec_reference);
    puzzle_free_cvec(&context, &cvec_compare);
    puzzle_free_context(&context);

	clock_t end = clock();
	double eltime = double(end - begin) / CLOCKS_PER_SEC;

    if (opts.printToFile) {
    	// Print to file
		ofstream outf;
		outf.open(opts.outFile, ofstream::out);
		outf.precision(3);

	    outf << endl << " *** Pictures found to be similar to " << opts.reference_pic << " *** " << endl << endl;
	    for(iter=topSimiliar.begin(); iter!=topSimiliar.end();++iter) {
	    	outf << iter->distance << " " << iter->name << endl;
	    }
		
	    outf << endl << endl << " *** Pictures found to be identical/close resemblance to " << opts.reference_pic << " *** " << endl << endl;
	    for(iter=topIdentical.begin(); iter!=topIdentical.end();++iter) {
	    	outf << iter->distance << " " << iter->name << endl;
	    }
	    outf << endl;

	    outf.close();

	} else {
		// Print in command line
	    cout.precision(3);

	    cout << endl << " *** Pictures found to be similar to " << opts.reference_pic << " *** " << endl << endl;
	    for(iter=topSimiliar.begin(); iter!=topSimiliar.end();++iter) {
	    	cout << iter->distance << " " << iter->name << endl;
	    }
		
	    cout << endl << endl << " *** Pictures found to be identical/close resemblance to " << opts.reference_pic << " *** " << endl << endl;
	    for(iter=topIdentical.begin(); iter!=topIdentical.end();++iter) {
	    	cout << iter->distance << " " << iter->name << endl;
	    }
	    cout << endl;


		cout << eltime;
	}

    return 0;
	
}