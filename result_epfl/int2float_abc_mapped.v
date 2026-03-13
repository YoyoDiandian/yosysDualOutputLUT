// Benchmark "/Users/yoyowang/Desktop/benchmark/int2float" written by ABC on Fri Mar 13 13:12:51 2026

module \/Users/yoyowang/Desktop/benchmark/int2float  ( 
    \B[0] , \B[1] , \B[2] , \B[3] , \B[4] , \B[5] , \B[6] , \B[7] , \B[8] ,
    \B[9] , \B[10] ,
    \M[0] , \M[1] , \M[2] , \M[3] , \E[0] , \E[1] , \E[2]   );
  input  \B[0] , \B[1] , \B[2] , \B[3] , \B[4] , \B[5] , \B[6] , \B[7] ,
    \B[8] , \B[9] , \B[10] ;
  output \M[0] , \M[1] , \M[2] , \M[3] , \E[0] , \E[1] , \E[2] ;
  wire new_n21, new_n22, new_n23, new_n24, new_n25, new_n26, new_n27,
    new_n29, new_n30, new_n31, new_n32, new_n33, new_n34, new_n35, new_n36,
    new_n37, new_n38, new_n39, new_n40, new_n41, new_n42, new_n43, new_n44,
    new_n46, new_n47, new_n48, new_n49, new_n50, new_n51, new_n52, new_n54,
    new_n56, new_n57, new_n58, new_n59, new_n60, new_n61, new_n63, new_n64,
    new_n65, new_n66, new_n68;
  assign \M[0]  = (~\B[10]  & (new_n21 | ~new_n24)) | (~new_n27 & \B[6] ) | (\B[7]  & ~\B[6]  & \B[10] );
  assign new_n21 = ~\B[9]  & ((~new_n22 & ~\B[5] ) | (~new_n23 & ~\B[8] ) | (~\B[4]  & \B[5]  & \B[8] ));
  assign new_n22 = (\B[7]  | \B[6]  | (\B[0]  ? (\B[4]  ? \B[1]  : \B[8] ) : (~\B[1]  | ~\B[4] ))) & (~\B[4]  | ~\B[8] );
  assign new_n23 = \B[3]  ? ((\B[4]  | ~\B[7] ) & (~\B[1]  | \B[2]  | ~\B[5]  | \B[7] )) : (~\B[4]  | ~\B[7] );
  assign new_n24 = \B[5]  ? (\B[6]  | (~new_n25 & ~new_n26 & ~\B[9] )) : (~\B[6]  | ~\B[9] );
  assign new_n25 = \B[1]  & ~\B[2]  & ((~\B[3]  & \B[4]  & ~\B[8] ) | (~\B[4]  & ~\B[7] ));
  assign new_n26 = ~\B[1]  & \B[2]  & ~\B[7]  & ~\B[8] ;
  assign new_n27 = (\B[7]  | (~\B[10]  & (\B[8]  | \B[9]  | (~\B[2]  ^ \B[3] )))) & (~\B[8]  | ~\B[9]  | ~\B[10] );
  assign \M[1]  = ~new_n38 & ~new_n43 & (\B[10]  | (~new_n29 & ~new_n34 & ~new_n44));
  assign new_n29 = ~\B[5]  & (~new_n33 | (~\B[6]  & (new_n30 | new_n31 | new_n32)));
  assign new_n30 = ~\B[1]  & ((~\B[4]  & ~\B[9] ) | (~\B[2]  & ~\B[7] ));
  assign new_n31 = \B[4]  & ~\B[7]  & (\B[0]  ? (\B[1]  & \B[2] ) : ~\B[2] );
  assign new_n32 = \B[8]  & ~\B[9] ;
  assign new_n33 = (\B[8]  | \B[9]  | ~\B[7]  | (\B[3]  & \B[4] )) & (\B[7]  | ~\B[9] );
  assign new_n34 = \B[5]  & (new_n37 | (\B[3]  & (new_n36 | (new_n35 & \B[1] ))));
  assign new_n35 = \B[2]  & (\B[4]  ? (~\B[8]  & ~\B[9] ) : (~\B[6]  & ~\B[7] ));
  assign new_n36 = \B[4]  & ~\B[8]  & \B[7]  & ~\B[9] ;
  assign new_n37 = \B[6]  & ((\B[4]  & \B[8]  & ~\B[9] ) | (\B[7]  & \B[9] ));
  assign new_n38 = ~\B[8]  & (new_n39 | (~\B[7]  & (new_n40 | new_n41 | ~new_n42)));
  assign new_n39 = ~\B[6]  & \B[10] ;
  assign new_n40 = ~\B[2]  & ((~\B[4]  & \B[6]  & ~\B[9] ) | (~\B[3]  & \B[5]  & ~\B[6] ));
  assign new_n41 = ~\B[3]  & ((~\B[4]  & \B[6]  & ~\B[9] ) | (~\B[1]  & \B[5]  & ~\B[6] ));
  assign new_n42 = ~\B[10]  & (~\B[2]  | ~\B[3]  | ~\B[4]  | ~\B[6]  | \B[9] );
  assign new_n43 = new_n32 & \B[7]  & \B[6]  & \B[10] ;
  assign new_n44 = ~\B[6]  & ((~\B[7]  & \B[9] ) | (~\B[4]  & \B[8]  & ~\B[9] ));
  assign \M[2]  = ~new_n52 | (~\B[10]  & (new_n51 | (~\B[9]  & (new_n46 | ~new_n50))));
  assign new_n46 = ~\B[8]  & (~new_n49 | (~\B[7]  & (new_n48 | (~new_n47 & \B[2] ))));
  assign new_n47 = (~\B[1]  | ((~\B[4]  | \B[6]  | ~\B[0]  | \B[3] ) & (~\B[3]  | \B[4]  | ~\B[5] ))) & (\B[5]  | ((\B[4]  | \B[6] ) & (~\B[3]  | ~\B[4]  | (\B[0]  & \B[1] ))));
  assign new_n48 = \B[4]  & ((~\B[2]  & \B[3]  & ~\B[6] ) | (~\B[3]  & \B[5] ));
  assign new_n49 = (~\B[3]  | ~\B[4]  | ((\B[1]  | ~\B[5]  | \B[6] ) & (~\B[2]  | \B[5]  | ~\B[6] ))) & (~\B[6]  | ~\B[5]  | (\B[3]  & \B[4] ));
  assign new_n50 = (~\B[4]  | ~\B[5]  | ((\B[2]  | ~\B[6]  | \B[7] ) & (~\B[3]  | \B[6]  | ~\B[7] ))) & (~\B[6]  | ~\B[7]  | (\B[4]  & \B[5] ));
  assign new_n51 = \B[8]  & ((~\B[6]  & \B[7] ) | (\B[5]  & ~\B[7]  & \B[4]  & \B[6] ));
  assign new_n52 = (~\B[6]  | ~\B[7]  | ((~\B[8]  | ~\B[10] ) & (~\B[5]  | \B[8]  | ~\B[9] ))) & (~\B[9]  | (~\B[10]  & (~\B[8]  | (\B[5]  & \B[7] ))));
  assign \M[3]  = \B[3]  | \B[10]  | new_n54 | \B[9] ;
  assign new_n54 = (\B[2]  | ~\B[6]  | ~\B[7]  | ~\B[5]  | ~\B[4]  | ~\B[8] ) & (\B[6]  | \B[7]  | \B[5]  | \B[4]  | \B[8] );
  assign \E[0]  = \B[10]  | (new_n60 & (\B[8]  | (new_n58 & (~new_n56 | ~\B[2] ))));
  assign new_n56 = ~new_n57 & \B[3] ;
  assign new_n57 = (~\B[5]  | ~\B[6]  | ~\B[4]  | \B[7] ) & (\B[5]  | \B[6]  | ~\B[0]  | ~\B[1] );
  assign new_n58 = (\B[4]  | (\B[6]  & ~\B[7] )) & (\B[3]  | (~\B[7]  & (~\B[5]  | \B[6] ))) & new_n59 & (~\B[7]  | (\B[5]  & \B[6] ));
  assign new_n59 = ~\B[9]  & (~\B[5]  | \B[6]  | (\B[1]  & \B[2] ));
  assign new_n60 = (~\B[4]  | \B[9]  | ~\B[7]  | ~\B[5]  | new_n61 | ~\B[6] ) & (~\B[9]  | (\B[7]  & \B[5]  & \B[6] ));
  assign new_n61 = \B[3]  ? ~\B[8]  : ~\B[2] ;
  assign \E[1]  = (~new_n63 & \B[4] ) | \B[9]  | \B[10]  | (new_n66 & ~\B[8] );
  assign new_n63 = (~\B[2]  | (~new_n65 & (~new_n64 | ~\B[6]  | ~\B[8] ))) & (~\B[3]  | ~\B[6]  | ~new_n64 | ~\B[8] );
  assign new_n64 = \B[5]  & \B[7] ;
  assign new_n65 = \B[0]  & \B[1]  & \B[3]  & ~\B[8]  & ~\B[5]  & ~\B[7] ;
  assign new_n66 = ~\B[7]  & (\B[5]  ? (~\B[2]  | ~\B[3]  | ~\B[4]  | ~\B[6] ) : \B[6] );
  assign \E[2]  = \B[7]  | \B[8]  | ~new_n68 | \B[10] ;
  assign new_n68 = ~\B[9]  & (~\B[5]  | ~\B[6]  | ~\B[2]  | ~\B[3]  | ~\B[4] );
endmodule


