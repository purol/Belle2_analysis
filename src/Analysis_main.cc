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

    Loader loader();
    loader.Load("./", "CHG.root", "CHG");
    loader.Load("./", "MIX.root", "MIX");
    loader.DrawStack();
    */

    Loader loader;
    loader.Load("./", ".root", "MC");
    loader.PrintInformation("========== initial ==========");
    loader.Cut("Btag_chiProb > 0.2");
    loader.PrintInformation("========== Btag_chiProb > 0.2 ==========");
    loader.DrawTH1D("Btag_chiProb^2", ";Btag chiProb square;", 30, 0.0, 1.0, "Btag_chiProb_square.png");
    loader.DrawTH2D("Btag_Mbc", "Btag_deltaE", ";Mbc;deltaE;", 30, 5.27, 5.29, 30, -0.2, 0.2, "Mbc_deltaE.png");
    loader.Cut("Btag_deltaE > (-15) * Btag_Mbc + 79.15");
    loader.DrawTH2D("Btag_Mbc", "Btag_deltaE", ";Mbc;deltaE;", 30, 5.27, 5.29, 30, -0.2, 0.2, "Mbc_deltaE_after_cut.png");
    loader.end();

    return 0;
}
