/* -*- c++ -*- */
/* 
 * Copyright 2013 <Bogdan Diaconescu, yo3iiu@yo3iiu.ro>.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include "inner_coder_impl.h"
#include <stdio.h>
#include <assert.h>

namespace gr {
  namespace dvbt {

    void
    inner_coder_impl::generate_codeword(unsigned char in, int &x, int &y)
    {
      //insert input bit
      d_reg |= ((in & 0x1) << 7);

      d_reg  = d_reg >> 1;

      // TODO - do this with polynoms and bitcnt
      //generate output G1=171(OCT)
      x = ((d_reg >> 6) ^ (d_reg >> 5) ^ (d_reg >> 4) ^ \
                        (d_reg >> 3) ^ d_reg) & 0x1;
      //generate output G2=133(OCT)
      y = ((d_reg >> 6) ^ (d_reg >> 4) ^ (d_reg >> 3) ^ \
                        (d_reg >> 1) ^ d_reg) & 0x1;
    }

    //TODO - do this based on puncturing matrix
    /*
     * Input e.g rate 2/3:
     * 000000x0x1
     * Output e.g. rate 2/3
     * 00000c0c1c2
     */
    void
    inner_coder_impl::generate_punctured_code(dvbt_code_rate_t coderate, unsigned char * in, unsigned char * out)
    {
      int x, y;

      switch(coderate)
      {
        //X1Y1
        case gr::dvbt::C1_2:
          generate_codeword(in[0], x, y);
          out[0] = x; out[1] = y;
          break;
        //X1Y1Y2
        case gr::dvbt::C2_3:
          generate_codeword(in[0], x, y);
          out[0] = x; out[1] = y;
          generate_codeword(in[1], x, y);
          out[2] = y;
          break;
        //X1Y1Y2X3
        case gr::dvbt::C3_4:
          generate_codeword(in[0], x, y);
          out[0] = x; out[1] = y;
          generate_codeword(in[1], x, y);
          out[2] = y;
          generate_codeword(in[2], x, y);
          out[3] = x;
          break;
        //X1Y1Y2X3Y4X5
        case gr::dvbt::C5_6:
          generate_codeword(in[0], x, y);
          out[0] = x; out[1] = y;
          generate_codeword(in[1], x, y);
          out[2] = y;
          generate_codeword(in[2], x, y);
          out[3] = x;
          generate_codeword(in[3], x, y);
          out[4] = y;
          generate_codeword(in[4], x, y);
          out[5] = x;
          break;
        //X1Y1Y2X3Y4X5Y6X7
        case gr::dvbt::C7_8:
          generate_codeword(in[0], x, y);
          out[0] = x; out[1] = y;
          generate_codeword(in[1], x, y);
          out[2] = y;
          generate_codeword(in[2], x, y);
          out[3] = y;
          generate_codeword(in[3], x, y);
          out[4] = y;
          generate_codeword(in[4], x, y);
          out[5] = x;
          generate_codeword(in[5], x, y);
          out[6] = y;
          generate_codeword(in[6], x, y);
          out[7] = x;
          break;
        default:
          generate_codeword(in[0], x, y);
          out[0] = x; out[1] = y;
          break;
      }

    }

    inner_coder::sptr
    inner_coder::make(int ninput, int noutput, dvbt_constellation_t constellation, \
        dvbt_hierarchy_t hierarchy, dvbt_code_rate_t coderate)
    {
      return gnuradio::get_initial_sptr (new inner_coder_impl(ninput, noutput, constellation, \
        hierarchy, coderate));
    }

    /*
     * The private constructor
     */
    inner_coder_impl::inner_coder_impl(int ninput, int noutput, dvbt_constellation_t constellation, \
        dvbt_hierarchy_t hierarchy, dvbt_code_rate_t coderate)
      : block("inner_coder",
          io_signature::make(1, 1, sizeof (unsigned char)),
          io_signature::make(1, 1, sizeof (unsigned char) * noutput)),
      config(constellation, hierarchy, coderate, coderate),
      d_ninput(ninput), d_noutput(noutput),
      d_reg(0),
      d_bitcount(0)
    {
      //Determine k - input of encoder
      d_k = config.d_cr_k;
      //Determine n - output of encoder
      d_n = config.d_cr_n;
      //Determine m - constellation symbol size
      d_m = config.d_m;

      printf("Inner_coder: d_k: %i, d_n: %i, d_m: %i\n", d_k, d_n, d_m);
      printf("Inner_coder: d_noutput: %i, d_ninput: %i\n", d_noutput, d_ninput);

      // In order to accomodate all constalations (m=2,4,6)
      // and rates (1/2, 2/3, 3/4, 5/6, 7/8)
      // We need the following things to happen:
      // - output item size multiple of 1512bytes
      // - noutput_items multiple of 4
      // - in block size 4*(k*m/8)
      // - out block size 4*n
      //
      // Rate calculation follows:
      // We process km input bits(km/8 Bytes)
      // We output nm bits
      // We output one byte for a symbol of m bits
      // The out/in rate in bytes is: 8n/km (Bytes)

      assert(d_ninput % 1);
      assert(d_noutput % 1512);

      // Set output items multiple of 4
      set_output_multiple(4);

      // Set relative rate out/in
      assert((d_noutput * d_k * d_m) % (d_ninput * 8 * d_n));
      set_relative_rate((float)(d_ninput * 8 * d_n) / (float)d_noutput * d_k * d_m);

      // calculate in and out block sizes
      d_in_bs = (d_k * d_m) / 2;
      d_out_bs = 4 * d_n;

      // allocate bit buffers
      d_in_buff = new unsigned char[8 * d_in_bs];
      if (d_in_buff == NULL)
      {
        std::cout << "Error allocating d_in_buff" << std::endl;
      }

      d_out_buff = new unsigned char[8 * d_in_bs * d_n / d_k];
      if (d_in_buff == NULL)
      {
        std::cout << "Error allocating d_out_buff" << std::endl;
      }
    }

    /*
     * Our virtual destructor.
     */
    inner_coder_impl::~inner_coder_impl()
    {
      delete [] d_out_buff;
      delete [] d_in_buff;
    }

    void
    inner_coder_impl::forecast (int noutput_items, gr_vector_int &ninput_items_required)
    {
       int input_required = noutput_items * d_noutput * d_k * d_m / (d_ninput * 8 * d_n);

       unsigned ninputs = ninput_items_required.size();
       for (unsigned int i = 0; i < ninputs; i++) {
         ninput_items_required[i] = input_required;
       }
    }

    int
    inner_coder_impl::general_work (int noutput_items,
                       gr_vector_int &ninput_items,
                       gr_vector_const_void_star &input_items,
                       gr_vector_void_star &output_items)
    {
        const unsigned char *in = (const unsigned char *) input_items[0];
        unsigned char *out = (unsigned char *) output_items[0];

        for (int k = 0; k < (noutput_items * d_noutput / d_out_bs); k++)
        {
          // Unpack input to bits
          for (int i = 0; i < d_in_bs; i++)
          {
            for (int j = 0; j < 8; j++)
            {
              d_in_buff[8*i + j] = (in[k*d_in_bs + i] >> (7 - j)) & 1;
            }
            //printf("in[%i]: %x\n", k*d_in_bs + i, in[k*d_in_bs + i]);
          }


          // Encode the data
          for (int in_bit = 0, out_bit = 0; in_bit < (8 * d_in_bs); in_bit += d_k, out_bit += d_n)
            generate_punctured_code(config.d_code_rate_HP, &d_in_buff[in_bit], &d_out_buff[out_bit]);

          // Pack d_m bit in one output byte
          for (int i = 0; i < d_out_bs; i++)
          {
            unsigned char c = 0;

            for (int j = 0; j < d_m; j++)
              c |= d_out_buff[d_m*i + j] << (d_m - 1 - j);

            out[k*d_out_bs + i] = c;

            //printf("out[%i]: %x\n", k*d_out_bs + i, out[k*d_out_bs + i]);
          }
        }

        // Tell runtime system how many input items we consumed on
        // each input stream.
        consume_each(noutput_items * d_noutput * d_k * d_m / (d_ninput * 8 * d_n));

        // Tell runtime system how many output items we produced.
        return noutput_items;
    }

  } /* namespace dvbt */
} /* namespace gr */

