#include <stdio.h>
#include <string>
#include <vector>

#include "TFile.h"

#include "Loader.h"

int main(int argc, char* argv[]) {


    /*
    Loader loader;
    loader.initialize();
    
    loader.load(Ntuple_path);
    loader.PrintInformation("========== inital ==========");
    loader.cut("Mbc > 5.27");
    loader.PrintInformation("========== Mbc > 5.27 ==========");
    loader.BCS("Mbc", "highest");
    auto output_loader = loader.save();
    loader.end();

    Loader loader_another;
    loader_another.initialize();
    loader_another.load(Ntuple_path);
    loader_another.cut("Mbc > 5.27");
    auto output_loader_another = loader_another.save();
    loader_another.end();

    Loader loader_other;
    loader_other.initialize();
    loader_other.load(output_loader);
    loader_other.load(output_loader_another);
    loader_other.cut("Mbc > 5.26");
    loader_other.PrintSeparateRootFile();
    loader_other.end();
    */

    Loader loader("./", ".root");
    loader.PrintInformation("========== initial ==========");
    loader.Cut("chiProb > 0.2");
    loader.PrintInformation("========== chiProb > 0.2 ==========");
    loader.DrawTH1D("chiProb_hist", ";chiProb;", "chiProb", 30, 0.2, 1.0, "chiProb.png");
    loader.end();

    return 0;
}
