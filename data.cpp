/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2014-2016 Gad Abraham
 * All rights reserved.
 */


#include "data.hpp"

Data::Data(long seed)
{
   N = 0;
   p = 0;
   K = 0;
   nsnps = 0;
   this->seed = seed;
   srand48(seed);
   visited = NULL;
}

Data::~Data()
{
   if(visited)
      delete[] visited;
   delete[] tmp;
   delete[] tmp2;
   delete[] avg;
   in.close();
}

/* 
 *                   plink BED           sparsnp
 * minor homozyous:  00 => numeric 0     10 => numeric 2
 * heterozygous:     10 => numeric 2     01 => numeric 1
 * major homozygous: 11 => numeric 3     00 => numeric 0
 * missing:          01 => numeric 1     11 => numeric 3
 *
 *
 * http://pngu.mgh.harvard.edu/~purcell/plink/binary.shtml says,
 * The bytes in plink are read backwards HGFEDCBA, not GHEFCDAB, but we read
 * them forwards as a character (a proper byte)
 *
 * By default, plink usage dosage of the *major* allele, since allele A1 is
 * usually the minor allele and the code "1" refers to the second allele A2,
 * so that "11" is A2/A2 or major/major.
 *
 * We always use minor allele dosage, to be consistent with the output from
 * plink --recodeA which used minor allele dosage by default.
 *
 * out: array of genotypes
 * in: array of packed genotypes (bytes)
 * n: number of bytes in input
 * 
 */
void decode_plink(unsigned char * __restrict__ out,
   const unsigned char * __restrict__ in,
   const unsigned int n)
{
   unsigned int i, k;
   unsigned char tmp, geno1, geno2, geno3, geno4;
   unsigned int a1, a2;

   for(i = 0 ; i < n ; i++)
   {
      tmp = in[i];
      k = PACK_DENSITY * i;

      /* geno is interpreted as a char, however a1 and a2 are bits for allele 1 and
       * allele 2. The final genotype is the sum of the alleles, except for 01
       * which denotes missing.
       */
      //geno1 = (tmp & MASK0);
      //a1 = !(geno1 & 1);
      //a2 = !(geno1 >> 1);
      //out[k] = (geno1 == 1) ? 3 : a1 + a2;

      //geno2 = (tmp & MASK1) >> 2; 
      //a1 = !(geno2 & 1);
      //a2 = !(geno2 >> 1);
      //out[k+1] = (geno2 == 1) ? 3 : a1 + a2;

      //geno3 = (tmp & MASK2) >> 4; 
      //a1 = !(geno3 & 1);
      //a2 = !(geno3 >> 1);
      //out[k+2] = (geno3 == 1) ? 3 : a1 + a2;

      //geno4 = (tmp & MASK3) >> 6; 
      //a1 = !(geno4 & 1);
      //a2 = !(geno4 >> 1);
      //out[k+3] = (geno4 == 1) ? 3 : a1 + a2;

      geno1 = (tmp & MASK0);
      if(geno1 == 1)
	 out[k] = 3;
      else
      {
	 a1 = !(geno1 & 1);
	 a2 = !(geno1 >> 1);
	 out[k] = a1 + a2;
      }

      geno2 = (tmp & MASK1) >> 2;
      if(geno2 == 1)
	 out[k+1] = 3;
      else
      {
	 a1 = !(geno2 & 1);
	 a2 = !(geno2 >> 1);
	 out[k+1] = a1 + a2;
      }

      geno3 = (tmp & MASK2) >> 4;
      if(geno3 == 1)
	 out[k] = 3;
      else
      {
	 a1 = !(geno3 & 1);
	 a2 = !(geno3 >> 1);
	 out[k+2] = a1 + a2;
      }

      geno4 = (tmp & MASK3) >> 6;
      if(geno4 == 1)
	 out[k] = 3;
      else
      {
	 a1 = !(geno4 & 1);
	 a2 = !(geno4 >> 1);
	 out[k+3] = a1 + a2;
      }
   }
}

void Data::get_size()
{
   std::cout << timestamp() << " Analyzing BED file '" 
      << geno_filename << "'";
   std::ifstream in(geno_filename, std::ios::in | std::ios::binary);

   if(!in)
   {
      std::cerr << "[Data::read_bed] Error reading file " 
	 << geno_filename << ", error " << strerror(errno) << std::endl <<
	 std::flush;
      throw std::runtime_error("io error");
   }

   in.seekg(0, std::ifstream::end);

   // file size in bytes, ignoring first 3 bytes (2byte magic number + 1byte mode)
   len = (unsigned long long)in.tellg() - 3;

   // size of packed data, in bytes, per SNP
   np = (unsigned long long)ceil((double)N / PACK_DENSITY);
   nsnps = (unsigned int)(len / np);
   in.seekg(3, std::ifstream::beg);
   in.close();

   std::cout << ", found " << (len + 3) << " bytes, "
      << nsnps << " SNPs" << std::endl;
}

// Prepare input stream etc before reading in SNP blocks
void Data::prepare()
{
   in.open(geno_filename, std::ios::in | std::ios::binary);
   in.seekg(3, std::ifstream::beg);

   if(!in)
   {
      std::cerr << "[Data::read_bed] Error reading file "
	 << geno_filename << std::endl;
      throw std::runtime_error("io error");
   }

   tmp = new unsigned char[np];

   // Allocate more than the sample size since data must take up whole bytes
   tmp2 = new unsigned char[np * PACK_DENSITY];

   avg = new double[nsnps](); 
   visited = new bool[nsnps]();

   scaled_geno_lookup = ArrayXXd::Zero(nsnps, 4);

   std::cout << timestamp() << " Detected BED file: "
      << geno_filename << " with " << (len + 3)
      << " bytes, " << N << " samples, " << nsnps 
      << " SNPs." << std::endl;
}

// Reads a _contiguous_ block of SNPs [start, stop] at a time.
// The block will contain standardised genotypes already, no need to
// standardise them again.
//
// If resize is false, then the calling code is responsible for ensuring that
// X is handled accordingly with the block size (X may be bigger than the
// block).
void Data::read_snp_block(unsigned int start_idx, unsigned int stop_idx,
   bool transpose, bool resize)
{
   in.seekg(3 + np * start_idx);

   unsigned int actual_block_size = stop_idx - start_idx + 1;

   // Resize matrix, e.g., with final block that may be smaller than
   // $blocksize$
   if(transpose)
   {
      if(X.rows() == 0 || (resize && X.rows() != actual_block_size))
      {
         std::cout << "reallocating memory: " << X.rows() << " -> " <<
	    actual_block_size << std::endl;
         if(X.rows() > actual_block_size)
            X = MatrixXd(actual_block_size, N);
      }
   }
   else if(X.cols() == 0 || (resize && X.cols() != actual_block_size))
   {
      std::cout << "reallocating memory: " << X.cols() << " -> " <<
	 actual_block_size << std::endl;
      X = MatrixXd(N, actual_block_size);
   }

   for(unsigned int j = 0; j < actual_block_size; j++)
   {
      unsigned int k = start_idx + j;

      // read raw genotypes
      in.read((char*)tmp, sizeof(char) * np);

      // decode the genotypes
      decode_plink(tmp2, tmp, np);

      // Compute average per SNP, excluding missing values
      double snp_avg = 0;
      unsigned int ngood = 0;

      // We've seen this SNP, don't need to compute its average again
      if(!visited[k])
      {
	 for(unsigned int i = 0 ; i < N ; i++)
      	 {
      	    double s = (double)tmp2[i];
      	    if(tmp2[i] != PLINK_NA)
      	    {
      	       snp_avg += s;
      	       ngood++;
      	    }
      	 }
      	 snp_avg /= ngood;

	 // Store the 4 possible standardized genotypes for each SNP
	 double P = snp_avg / 2.0;
	 double sd = sqrt(2.0 * P * (1 - P));
	 if(sd > VAR_TOL)
	 {
	    scaled_geno_lookup(k, 0) = (0 - snp_avg) / sd;
	    scaled_geno_lookup(k, 1) = (1 - snp_avg) / sd;
	    scaled_geno_lookup(k, 2) = (2 - snp_avg) / sd;
	    scaled_geno_lookup(k, 3) = 0; // Assumes PLINK NA = 3
	 }
	 visited[k] = true;
      }

      for(unsigned int i = 0 ; i < N ; i++)
      {
	 X(i, j) = scaled_geno_lookup(k, tmp2[i]);
      }
   }
}

// Reads an entire bed file into memory
// Expects PLINK bed in SNP-major format
void Data::read_bed(bool transpose)
{
   if(transpose)
      X = MatrixXd(nsnps, N);
   else
      X = MatrixXd(N, nsnps);

   //std::cout << timestamp() << " Detected BED file: "
   //   << geno_filename << " with " << (len + 3)
   //   << " bytes, " << N << " samples, " << nsnps 
   //   << " SNPs." << std::endl;

   unsigned int md = nsnps / 50;

   // iterate over all SNPs
   for(unsigned int j = 0 ; j < nsnps; j++)
   {
      // read raw genotypes
      in.read((char*)tmp, sizeof(char) * np);

      // decode the genotypes
      decode_plink(tmp2, tmp, np);

      // Compute average per SNP, excluding missing values
      avg[j] = 0;
      unsigned int ngood = 0;
      for(unsigned int i = 0 ; i < N ; i++)
      {
	 double s = (double)tmp2[i];
	 if(tmp2[i] != PLINK_NA)
	 {
	    avg[j] += s;
	    ngood++;
	 }
      }
      avg[j] /= ngood;

      // Impute using average per SNP
      for(unsigned int i = 0 ; i < N ; i++)
      {
	 double s = (double)tmp2[i];
	 {
	    if(transpose)
	    {
	       if(s != PLINK_NA)
		  X(j, i) = s;
	       else
		  X(j, i) = avg[j];
	    }
	    else
	    {
	       if(s != PLINK_NA)
		  X(i, j) = s;
	       else
		  X(i, j) = avg[j];
	    }
	 }
      }

      if(verbose && j % md == md - 1)
	 std::cout << timestamp() << " Reading genotypes, "
	    << roundl(((double)j / nsnps) * 100) << "% done" 
	    << std::endl;
   }

   if(transpose)
      p = X.rows();
   else
      p = X.cols();

   std::cout << timestamp() << " Loaded genotypes: "
      << N << " samples, " << p << " SNPs" << std::endl;
}

void Data::read_pheno(const char *filename, unsigned int firstcol)
{
   Y = read_plink_pheno(filename, firstcol);
}

// Reads PLINK phenotype files:
// FID IID pheno1 pheno2 ...
// Need to be able to read continuous phenotypes
// 
// firstcol is one-based, 3 for pheno file, 6 for FAM file (ignoring gender),
// 5 for FAM file (with gender)
MatrixXd Data::read_plink_pheno(const char *filename, unsigned int firstcol)
{
   std::ifstream in(filename, std::ios::in);

   if(!in)
   {
      std::cerr << "[Data::read_plink_pheno] Error reading file " 
	 << filename << std::endl;
      throw std::string("io error");
   }
   std::vector<std::string> lines;
   
   while(in)
   {
      std::string line;
      std::getline(in, line);
      if(!in.eof())
	 lines.push_back(line);
   }

   std::cout << timestamp() << " Detected pheno file " <<
      filename << ", " << lines.size() << " samples" << std::endl;

   in.close();

   unsigned int numtok = 0, numfields;

   MatrixXd Z(0, 0);

   for(unsigned int i = 0 ; i < lines.size() ; i++)
   {
      std::stringstream ss(lines[i]);
      std::string s;
      std::vector<std::string> tokens;

      while(ss >> s)
	 tokens.push_back(s);

      numtok = tokens.size();
      numfields = numtok - firstcol + 1;
      if(i == 0)
	 Z.resize(lines.size(), numfields);

      VectorXd y(numfields);
      for(unsigned int j = 0 ; j < numfields ; j++)
	 y(j) = std::atof(tokens[j + firstcol - 1].c_str());
      Z.row(i) = y;
   }

   N = Z.rows();

   return Z;
}

void Data::read_plink_bim(const char *filename)
{
   std::ifstream in(filename, std::ios::in);

   if(!in)
   {
      std::cerr << "[Data::read_plink_bim] Error reading file " 
	 << filename << std::endl;
      throw std::string("io error");
   }
   std::vector<std::string> lines;
   
   while(in)
   {
      std::string line;
      std::getline(in, line);
      if(!in.eof())
	 lines.push_back(line);
   }

   std::cout << timestamp() << " Detected bim file " <<
      filename << ", " << lines.size() << " SNPs" << std::endl;
   in.close();

   for(unsigned int i = 0 ; i < lines.size() ; i++)
   {
      std::stringstream ss(lines[i]);
      std::string s;
      std::vector<std::string> tokens;

      while(ss >> s)
	 tokens.push_back(s);
      snp_ids.push_back(tokens[1]);
   }
}

std::string Data::tolower(const std::string& v)
{
   std::string r = v;
   std::transform(r.begin(), r.end(), r.begin(), ::tolower);
   return r;
}

