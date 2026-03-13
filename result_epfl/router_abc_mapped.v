// Benchmark "/Users/yoyowang/Desktop/benchmark/router" written by ABC on Fri Mar 13 13:20:04 2026

module \/Users/yoyowang/Desktop/benchmark/router  ( 
    \dest_x[0] , \dest_x[1] , \dest_x[2] , \dest_x[3] , \dest_x[4] ,
    \dest_x[5] , \dest_x[6] , \dest_x[7] , \dest_x[8] , \dest_x[9] ,
    \dest_x[10] , \dest_x[11] , \dest_x[12] , \dest_x[13] , \dest_x[14] ,
    \dest_x[15] , \dest_x[16] , \dest_x[17] , \dest_x[18] , \dest_x[19] ,
    \dest_x[20] , \dest_x[21] , \dest_x[22] , \dest_x[23] , \dest_x[24] ,
    \dest_x[25] , \dest_x[26] , \dest_x[27] , \dest_x[28] , \dest_x[29] ,
    \dest_y[0] , \dest_y[1] , \dest_y[2] , \dest_y[3] , \dest_y[4] ,
    \dest_y[5] , \dest_y[6] , \dest_y[7] , \dest_y[8] , \dest_y[9] ,
    \dest_y[10] , \dest_y[11] , \dest_y[12] , \dest_y[13] , \dest_y[14] ,
    \dest_y[15] , \dest_y[16] , \dest_y[17] , \dest_y[18] , \dest_y[19] ,
    \dest_y[20] , \dest_y[21] , \dest_y[22] , \dest_y[23] , \dest_y[24] ,
    \dest_y[25] , \dest_y[26] , \dest_y[27] , \dest_y[28] , \dest_y[29] ,
    \outport[0] , \outport[1] , \outport[2] , \outport[3] , \outport[4] ,
    \outport[5] , \outport[6] , \outport[7] , \outport[8] , \outport[9] ,
    \outport[10] , \outport[11] , \outport[12] , \outport[13] ,
    \outport[14] , \outport[15] , \outport[16] , \outport[17] ,
    \outport[18] , \outport[19] , \outport[20] , \outport[21] ,
    \outport[22] , \outport[23] , \outport[24] , \outport[25] ,
    \outport[26] , \outport[27] , \outport[28] , \outport[29]   );
  input  \dest_x[0] , \dest_x[1] , \dest_x[2] , \dest_x[3] , \dest_x[4] ,
    \dest_x[5] , \dest_x[6] , \dest_x[7] , \dest_x[8] , \dest_x[9] ,
    \dest_x[10] , \dest_x[11] , \dest_x[12] , \dest_x[13] , \dest_x[14] ,
    \dest_x[15] , \dest_x[16] , \dest_x[17] , \dest_x[18] , \dest_x[19] ,
    \dest_x[20] , \dest_x[21] , \dest_x[22] , \dest_x[23] , \dest_x[24] ,
    \dest_x[25] , \dest_x[26] , \dest_x[27] , \dest_x[28] , \dest_x[29] ,
    \dest_y[0] , \dest_y[1] , \dest_y[2] , \dest_y[3] , \dest_y[4] ,
    \dest_y[5] , \dest_y[6] , \dest_y[7] , \dest_y[8] , \dest_y[9] ,
    \dest_y[10] , \dest_y[11] , \dest_y[12] , \dest_y[13] , \dest_y[14] ,
    \dest_y[15] , \dest_y[16] , \dest_y[17] , \dest_y[18] , \dest_y[19] ,
    \dest_y[20] , \dest_y[21] , \dest_y[22] , \dest_y[23] , \dest_y[24] ,
    \dest_y[25] , \dest_y[26] , \dest_y[27] , \dest_y[28] , \dest_y[29] ;
  output \outport[0] , \outport[1] , \outport[2] , \outport[3] , \outport[4] ,
    \outport[5] , \outport[6] , \outport[7] , \outport[8] , \outport[9] ,
    \outport[10] , \outport[11] , \outport[12] , \outport[13] ,
    \outport[14] , \outport[15] , \outport[16] , \outport[17] ,
    \outport[18] , \outport[19] , \outport[20] , \outport[21] ,
    \outport[22] , \outport[23] , \outport[24] , \outport[25] ,
    \outport[26] , \outport[27] , \outport[28] , \outport[29] ;
  wire new_n93, new_n94, new_n95, new_n96, new_n97, new_n98, new_n99,
    new_n100, new_n101, new_n102, new_n103, new_n104, new_n105, new_n106,
    new_n107, new_n108, new_n109, new_n110, new_n111, new_n112, new_n113,
    new_n114, new_n115, new_n116, new_n117, new_n118, new_n119, new_n120,
    new_n121, new_n122, new_n123, new_n124, new_n125, new_n126, new_n127,
    new_n128, new_n129, new_n130, new_n132, new_n133, new_n134, new_n135,
    new_n136, new_n137, new_n138, new_n139, new_n140, new_n141, new_n142,
    new_n143, new_n144, new_n145, new_n146, new_n147, new_n148, new_n149,
    new_n150, new_n151, new_n152, new_n153, new_n154;
  assign \outport[0]  = new_n93 | new_n124;
  assign new_n93 = ~new_n123 & (~\dest_x[0]  | ~\dest_x[1]  | ~\dest_x[2]  | ~new_n94 | ~\dest_x[3] );
  assign new_n94 = \dest_x[4]  & \dest_x[5]  & \dest_x[6]  & \dest_x[7]  & new_n95 & \dest_x[8] ;
  assign new_n95 = ~new_n121 & new_n122 & ~new_n117 & ~new_n119 & new_n96 & new_n120;
  assign new_n96 = ~new_n112 & ~new_n113 & new_n114 & ~new_n115 & new_n97 & new_n116;
  assign new_n97 = ~new_n107 & ~new_n108 & ~new_n109 & new_n110 & new_n98 & ~new_n111;
  assign new_n98 = ~new_n99 & ~new_n103 & new_n104 & ~new_n105 & ~new_n106 & ~\dest_x[9] ;
  assign new_n99 = ~new_n100 ^ \dest_x[25] ;
  assign new_n100 = \dest_x[24]  & \dest_x[23]  & (\dest_x[22]  | \dest_x[21]  | (new_n101 & \dest_x[20] ));
  assign new_n101 = \dest_x[19]  & (\dest_x[18]  | (\dest_x[17]  & (\dest_x[16]  | (new_n102 & \dest_x[15] ))));
  assign new_n102 = \dest_x[14]  & (\dest_x[13]  | \dest_x[12]  | ((\dest_x[9]  | \dest_x[10] ) & \dest_x[11] ));
  assign new_n103 = ~\dest_x[26]  ^ (~new_n100 | ~\dest_x[25] );
  assign new_n104 = ~\dest_x[27]  ^ (~\dest_x[26]  & (~new_n100 | ~\dest_x[25] ));
  assign new_n105 = \dest_x[28]  ^ (~\dest_x[27]  | (~\dest_x[26]  & (~new_n100 | ~\dest_x[25] )));
  assign new_n106 = ~\dest_x[29]  ^ (\dest_x[28]  & \dest_x[27]  & (\dest_x[26]  | (new_n100 & \dest_x[25] )));
  assign new_n107 = ~new_n101 ^ \dest_x[20] ;
  assign new_n108 = ~\dest_x[21]  ^ (~new_n101 | ~\dest_x[20] );
  assign new_n109 = ~\dest_x[22]  ^ (~\dest_x[21]  & (~new_n101 | ~\dest_x[20] ));
  assign new_n110 = \dest_x[23]  ^ (\dest_x[22]  | \dest_x[21]  | (new_n101 & \dest_x[20] ));
  assign new_n111 = \dest_x[24]  ^ (~\dest_x[23]  | (~\dest_x[22]  & ~\dest_x[21]  & (~new_n101 | ~\dest_x[20] )));
  assign new_n112 = ~new_n102 ^ \dest_x[15] ;
  assign new_n113 = ~\dest_x[16]  ^ (~new_n102 | ~\dest_x[15] );
  assign new_n114 = ~\dest_x[17]  ^ (~\dest_x[16]  & (~new_n102 | ~\dest_x[15] ));
  assign new_n115 = ~\dest_x[18]  ^ (~\dest_x[17]  | (~\dest_x[16]  & (~new_n102 | ~\dest_x[15] )));
  assign new_n116 = \dest_x[19]  ^ (\dest_x[18]  | (\dest_x[17]  & (\dest_x[16]  | (new_n102 & \dest_x[15] ))));
  assign new_n117 = ~new_n118 ^ ~\dest_x[12] ;
  assign new_n118 = (\dest_x[9]  | \dest_x[10] ) & \dest_x[11] ;
  assign new_n119 = (new_n118 | \dest_x[12] ) ^ \dest_x[13] ;
  assign new_n120 = \dest_x[14]  ^ (new_n118 | \dest_x[12]  | \dest_x[13] );
  assign new_n121 = ~\dest_x[9]  ^ ~\dest_x[10] ;
  assign new_n122 = (\dest_x[9]  | \dest_x[10] ) ^ \dest_x[11] ;
  assign new_n123 = \dest_x[29]  & \dest_x[28]  & \dest_x[27]  & (\dest_x[26]  | (new_n100 & \dest_x[25] ));
  assign new_n124 = new_n123 & (~\dest_x[9]  | ~new_n105 | ~new_n125 | new_n104);
  assign new_n125 = new_n103 & new_n99 & new_n111 & new_n126 & ~new_n110;
  assign new_n126 = new_n109 & new_n108 & new_n107 & new_n127 & ~new_n116;
  assign new_n127 = new_n115 & ~new_n114 & new_n128 & new_n113;
  assign new_n128 = new_n112 & ~new_n120 & new_n119 & new_n117 & new_n129;
  assign new_n129 = ~new_n122 & new_n121 & ~\dest_x[8]  & new_n130 & ~\dest_x[7] ;
  assign new_n130 = ~\dest_x[6]  & ~\dest_x[5]  & ~\dest_x[4]  & ~\dest_x[1]  & ~\dest_x[2]  & ~\dest_x[3] ;
  assign \outport[1]  = ~new_n124 & ((~new_n154 & \dest_x[0] ) | new_n93 | (new_n132 & (~new_n154 | (~\dest_x[0]  & ~\dest_y[0] ))));
  assign new_n132 = ~new_n146 & (~\dest_y[1]  | ~\dest_y[2]  | ~new_n133 | ~\dest_y[3] );
  assign new_n133 = \dest_y[4]  & \dest_y[5]  & \dest_y[6]  & \dest_y[7]  & new_n134 & \dest_y[8] ;
  assign new_n134 = (\dest_y[9]  ^ ~\dest_y[10] ) & (~\dest_y[11]  | (~\dest_y[9]  & ~\dest_y[10] )) & (\dest_y[11]  | \dest_y[9]  | \dest_y[10] ) & (~\dest_y[12]  ^ (\dest_y[11]  & (\dest_y[9]  | \dest_y[10] ))) & new_n135 & (\dest_y[13]  ^ (~\dest_y[12]  & (~\dest_y[11]  | (~\dest_y[9]  & ~\dest_y[10] ))));
  assign new_n135 = (~new_n145 | \dest_y[14] ) & (new_n145 | ~\dest_y[14] ) & (~\dest_y[15]  ^ (new_n145 | ~\dest_y[14] )) & (\dest_y[16]  ^ (~\dest_y[15]  | new_n145 | ~\dest_y[14] )) & new_n136 & (~\dest_y[17]  | (~\dest_y[16]  & (~\dest_y[15]  | new_n145 | ~\dest_y[14] ))) & (\dest_y[17]  | \dest_y[16]  | (\dest_y[15]  & ~new_n145 & \dest_y[14] ));
  assign new_n136 = (new_n144 ^ ~\dest_y[18] ) & (~\dest_y[19]  | (~new_n144 & ~\dest_y[18] )) & (\dest_y[19]  | new_n144 | \dest_y[18] ) & (~\dest_y[20]  ^ (~\dest_y[19]  | (~new_n144 & ~\dest_y[18] ))) & new_n137 & (\dest_y[21]  ^ (~\dest_y[20]  | ~\dest_y[19]  | (~new_n144 & ~\dest_y[18] )));
  assign new_n137 = (~new_n143 ^ ~\dest_y[22] ) & (~\dest_y[23]  | (new_n143 & ~\dest_y[22] )) & (\dest_y[23]  | ~new_n143 | \dest_y[22] ) & new_n138 & (~\dest_y[24]  ^ (~\dest_y[23]  | (new_n143 & ~\dest_y[22] )));
  assign new_n138 = (new_n139 ^ \dest_y[25] ) & (\dest_y[26]  ^ (~new_n139 | ~\dest_y[25] )) & (~\dest_y[27]  | (~\dest_y[26]  & (~new_n139 | ~\dest_y[25] ))) & (\dest_y[27]  | \dest_y[26]  | (new_n139 & \dest_y[25] )) & new_n142 & (~\dest_y[28]  ^ (~\dest_y[27]  | (~\dest_y[26]  & (~new_n139 | ~\dest_y[25] ))));
  assign new_n139 = \dest_y[24]  & \dest_y[23]  & (\dest_y[22]  | \dest_y[21]  | (new_n140 & \dest_y[20] ));
  assign new_n140 = \dest_y[19]  & (\dest_y[18]  | (\dest_y[17]  & (\dest_y[16]  | (new_n141 & \dest_y[15] ))));
  assign new_n141 = \dest_y[14]  & (\dest_y[13]  | \dest_y[12]  | ((\dest_y[9]  | \dest_y[10] ) & \dest_y[11] ));
  assign new_n142 = \dest_y[29]  & \dest_y[0]  & ~\dest_y[9] ;
  assign new_n143 = ~\dest_y[21]  & (~new_n140 | ~\dest_y[20] );
  assign new_n144 = \dest_y[17]  & (\dest_y[16]  | (new_n141 & \dest_y[15] ));
  assign new_n145 = ~\dest_y[13]  & ~\dest_y[12]  & ((~\dest_y[9]  & ~\dest_y[10] ) | ~\dest_y[11] );
  assign new_n146 = new_n154 & (~new_n147 | ~\dest_y[9] );
  assign new_n147 = (~\dest_y[28]  | (\dest_y[27]  & (\dest_y[26]  | (new_n139 & \dest_y[25] )))) & (\dest_y[28]  | ~\dest_y[27]  | (~\dest_y[26]  & (~new_n139 | ~\dest_y[25] ))) & (~\dest_y[27]  ^ (\dest_y[26]  | (new_n139 & \dest_y[25] ))) & new_n148 & (~new_n139 | \dest_y[25] ) & (new_n139 | ~\dest_y[25] ) & (\dest_y[26]  | (new_n139 & \dest_y[25] )) & (~\dest_y[26]  | ~new_n139 | ~\dest_y[25] );
  assign new_n148 = (~\dest_y[24]  | (\dest_y[23]  & (~new_n143 | \dest_y[22] ))) & (\dest_y[24]  | ~\dest_y[23]  | (new_n143 & ~\dest_y[22] )) & (~\dest_y[23]  ^ (~new_n143 | \dest_y[22] )) & new_n149 & (~new_n143 | \dest_y[22] ) & (new_n143 | ~\dest_y[22] );
  assign new_n149 = (\dest_y[21]  | (\dest_y[20]  & \dest_y[19]  & (new_n144 | \dest_y[18] ))) & (~\dest_y[21]  | ~\dest_y[20]  | ~\dest_y[19]  | (~new_n144 & ~\dest_y[18] )) & (~\dest_y[20]  | (\dest_y[19]  & (new_n144 | \dest_y[18] ))) & (\dest_y[20]  | ~\dest_y[19]  | (~new_n144 & ~\dest_y[18] )) & (~\dest_y[19]  ^ (new_n144 | \dest_y[18] )) & new_n150 & (new_n144 | \dest_y[18] ) & (~new_n144 | ~\dest_y[18] );
  assign new_n150 = (~\dest_y[17]  ^ (\dest_y[16]  | (\dest_y[15]  & ~new_n145 & \dest_y[14] ))) & (\dest_y[16]  | (\dest_y[15]  & ~new_n145 & \dest_y[14] )) & (~\dest_y[16]  | ~\dest_y[15]  | new_n145 | ~\dest_y[14] ) & (~\dest_y[15]  | (~new_n145 & \dest_y[14] )) & (\dest_y[15]  | new_n145 | ~\dest_y[14] ) & new_n151 & (~new_n145 ^ ~\dest_y[14] );
  assign new_n151 = (\dest_y[13]  | \dest_y[12]  | (\dest_y[11]  & (\dest_y[9]  | \dest_y[10] ))) & (~\dest_y[13]  | (~\dest_y[12]  & (~\dest_y[11]  | (~\dest_y[9]  & ~\dest_y[10] )))) & (~\dest_y[12]  | ~\dest_y[11]  | (~\dest_y[9]  & ~\dest_y[10] )) & (\dest_y[12]  | (\dest_y[11]  & (\dest_y[9]  | \dest_y[10] ))) & (~\dest_y[11]  ^ (\dest_y[9]  | \dest_y[10] )) & new_n152 & (\dest_y[9]  | \dest_y[10] ) & (~\dest_y[9]  | ~\dest_y[10] );
  assign new_n152 = ~\dest_y[8]  & new_n153 & ~\dest_y[7] ;
  assign new_n153 = ~\dest_y[6]  & ~\dest_y[5]  & ~\dest_y[4]  & ~\dest_y[1]  & ~\dest_y[2]  & ~\dest_y[3] ;
  assign new_n154 = \dest_y[29]  & \dest_y[28]  & \dest_y[27]  & (\dest_y[26]  | (new_n139 & \dest_y[25] ));
  assign \outport[2]  = ~new_n93 & ~new_n124 & (new_n146 | (\dest_y[0]  & new_n154 & \dest_x[0] ));
  assign \outport[3]  = 1'b0;
  assign \outport[4]  = 1'b0;
  assign \outport[5]  = 1'b0;
  assign \outport[6]  = 1'b0;
  assign \outport[7]  = 1'b0;
  assign \outport[8]  = 1'b0;
  assign \outport[9]  = 1'b0;
  assign \outport[10]  = 1'b0;
  assign \outport[11]  = 1'b0;
  assign \outport[12]  = 1'b0;
  assign \outport[13]  = 1'b0;
  assign \outport[14]  = 1'b0;
  assign \outport[15]  = 1'b0;
  assign \outport[16]  = 1'b0;
  assign \outport[17]  = 1'b0;
  assign \outport[18]  = 1'b0;
  assign \outport[19]  = 1'b0;
  assign \outport[20]  = 1'b0;
  assign \outport[21]  = 1'b0;
  assign \outport[22]  = 1'b0;
  assign \outport[23]  = 1'b0;
  assign \outport[24]  = 1'b0;
  assign \outport[25]  = 1'b0;
  assign \outport[26]  = 1'b0;
  assign \outport[27]  = 1'b0;
  assign \outport[28]  = 1'b0;
  assign \outport[29]  = 1'b0;
endmodule


