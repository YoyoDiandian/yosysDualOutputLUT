// Benchmark "/Users/yoyowang/Desktop/benchmark/cavlc" written by ABC on Fri Mar 13 13:03:14 2026

module \/Users/yoyowang/Desktop/benchmark/cavlc  ( 
    \totalcoeffs[0] , \totalcoeffs[1] , \totalcoeffs[2] , \totalcoeffs[3] ,
    \totalcoeffs[4] , \ctable[0] , \ctable[1] , \ctable[2] ,
    \trailingones[0] , \trailingones[1] ,
    \coeff_token[0] , \coeff_token[1] , \coeff_token[2] , \coeff_token[3] ,
    \coeff_token[4] , \coeff_token[5] , \ctoken_len[0] , \ctoken_len[1] ,
    \ctoken_len[2] , \ctoken_len[3] , \ctoken_len[4]   );
  input  \totalcoeffs[0] , \totalcoeffs[1] , \totalcoeffs[2] ,
    \totalcoeffs[3] , \totalcoeffs[4] , \ctable[0] , \ctable[1] ,
    \ctable[2] , \trailingones[0] , \trailingones[1] ;
  output \coeff_token[0] , \coeff_token[1] , \coeff_token[2] ,
    \coeff_token[3] , \coeff_token[4] , \coeff_token[5] , \ctoken_len[0] ,
    \ctoken_len[1] , \ctoken_len[2] , \ctoken_len[3] , \ctoken_len[4] ;
  wire new_n24, new_n25, new_n26, new_n27, new_n28, new_n29, new_n30,
    new_n31, new_n32, new_n33, new_n34, new_n35, new_n36, new_n37, new_n38,
    new_n39, new_n40, new_n41, new_n42, new_n44, new_n45, new_n46, new_n47,
    new_n48, new_n49, new_n50, new_n51, new_n52, new_n53, new_n54, new_n55,
    new_n56, new_n57, new_n59, new_n60, new_n61, new_n62, new_n63, new_n64,
    new_n65, new_n66, new_n67, new_n68, new_n69, new_n70, new_n71, new_n72,
    new_n73, new_n75, new_n76, new_n77, new_n78, new_n79, new_n80, new_n81,
    new_n82, new_n83, new_n84, new_n85, new_n86, new_n88, new_n90, new_n92,
    new_n93, new_n94, new_n95, new_n96, new_n97, new_n98, new_n99,
    new_n100, new_n101, new_n102, new_n103, new_n104, new_n106, new_n107,
    new_n108, new_n109, new_n110, new_n111, new_n112, new_n113, new_n114,
    new_n115, new_n116, new_n117, new_n119, new_n120, new_n121, new_n122,
    new_n123, new_n124, new_n125, new_n126, new_n127, new_n129, new_n130,
    new_n131, new_n132, new_n133, new_n134, new_n135, new_n136, new_n137,
    new_n138, new_n139, new_n140, new_n141, new_n143, new_n144;
  assign \coeff_token[0]  = new_n41 | (~\totalcoeffs[4]  & ((~new_n31 & ~\ctable[2] ) | (~new_n24 & ~\ctable[1] )));
  assign new_n24 = (\trailingones[0]  | (~new_n25 & (new_n30 | \ctable[2] ))) & (new_n27 | \totalcoeffs[3] );
  assign new_n25 = ~\totalcoeffs[2]  & ((new_n26 & (\totalcoeffs[1]  | ~\trailingones[1] ) & (~\totalcoeffs[1]  | (~\totalcoeffs[0]  & \trailingones[1] ))) | ((\totalcoeffs[1]  | (\totalcoeffs[0]  & ~\trailingones[1] )) & ~\ctable[2]  & (~\totalcoeffs[1]  | \trailingones[1] )));
  assign new_n26 = ~\totalcoeffs[3]  & ~\ctable[0] ;
  assign new_n27 = (new_n29 | ~\totalcoeffs[0]  | \totalcoeffs[2] ) & (~new_n28 | ~\totalcoeffs[2]  | \trailingones[1]  | \totalcoeffs[0]  | \totalcoeffs[1] );
  assign new_n28 = ~\ctable[0]  & \ctable[2]  & \trailingones[0] ;
  assign new_n29 = (~\totalcoeffs[1]  | ((\ctable[0]  | ~\ctable[2]  | ~\trailingones[0] ) & (\ctable[2]  | ~\trailingones[1] ))) & ((\totalcoeffs[1]  & ~\ctable[2] ) | \ctable[0]  | \trailingones[1] );
  assign new_n30 = (\trailingones[1]  | ((\totalcoeffs[0]  | (~\totalcoeffs[2]  & ~\ctable[0] )) & (~\totalcoeffs[1]  | ~\totalcoeffs[2] ))) & (\totalcoeffs[1]  | ((~\trailingones[1]  | (~\totalcoeffs[2]  & ~\totalcoeffs[3] )) & (~\totalcoeffs[0]  | ~\totalcoeffs[3] ))) & (~\totalcoeffs[1]  | \totalcoeffs[3] );
  assign new_n31 = (~\totalcoeffs[3]  | (~new_n40 & (new_n39 | ~\trailingones[0] ))) & new_n35 & (new_n32 | ~\trailingones[0] );
  assign new_n32 = ~new_n34 & (new_n33 | ~\ctable[0] );
  assign new_n33 = (~\ctable[1]  | (~\totalcoeffs[2]  & (~\totalcoeffs[0]  | \trailingones[1] ))) & (\totalcoeffs[2]  | \totalcoeffs[3]  | ~\totalcoeffs[1]  | (~\totalcoeffs[0]  ^ ~\trailingones[1] ));
  assign new_n34 = \ctable[1]  & (\totalcoeffs[2]  ? \trailingones[1]  : (\totalcoeffs[1]  & ~\trailingones[1] )) & ~\totalcoeffs[0]  & ~\totalcoeffs[3] ;
  assign new_n35 = ~new_n38 & (\ctable[0]  | (~new_n36 & ~new_n37));
  assign new_n36 = ~\totalcoeffs[0]  & ((~\totalcoeffs[1]  & \trailingones[1]  & \totalcoeffs[2]  & ~\totalcoeffs[3] ) | (\totalcoeffs[1]  & ~\trailingones[0] ));
  assign new_n37 = ~\trailingones[0]  & ((\totalcoeffs[2]  & \trailingones[1] ) | (~\totalcoeffs[1]  & ~\trailingones[1]  & (~\totalcoeffs[3]  | (~\totalcoeffs[2]  & \ctable[1] ))));
  assign new_n38 = ~\totalcoeffs[0]  & ~\totalcoeffs[2]  & ~\totalcoeffs[3]  & ~\trailingones[0]  & ~\totalcoeffs[1]  & ~\trailingones[1] ;
  assign new_n39 = (~\ctable[1]  | (~\ctable[0]  & (\totalcoeffs[1]  | ~\trailingones[1]  | \totalcoeffs[0]  | \totalcoeffs[2] ))) & (~\ctable[0]  | ~\totalcoeffs[2]  | ~\totalcoeffs[1]  | (~\totalcoeffs[0]  ^ ~\trailingones[1] ));
  assign new_n40 = ~\ctable[0]  & ((~\trailingones[0]  & (\totalcoeffs[1]  | (~\totalcoeffs[0]  & \trailingones[1] ))) | (\totalcoeffs[0]  & ~\trailingones[1]  & ~\totalcoeffs[1]  & \totalcoeffs[2] ));
  assign new_n41 = ~\totalcoeffs[0]  & new_n42 & ~\totalcoeffs[2]  & ~\totalcoeffs[1]  & ~\totalcoeffs[3] ;
  assign new_n42 = ~\ctable[2]  & ((\ctable[0]  & (\ctable[1]  ? (\totalcoeffs[4]  & \trailingones[0] ) : (~\trailingones[0]  & ~\trailingones[1] ))) | (~\trailingones[0]  & (~\ctable[0]  | ~\ctable[1] ) & (\trailingones[1]  ? \totalcoeffs[4]  : \ctable[1] )));
  assign \coeff_token[1]  = new_n56 | (~\ctable[2]  & ((~new_n50 & ~\totalcoeffs[4] ) | (~new_n44 & ~\totalcoeffs[3] )));
  assign new_n44 = (\totalcoeffs[1]  | (~new_n45 & (new_n47 | \totalcoeffs[4] ))) & (\totalcoeffs[4]  | (~new_n49 & (new_n48 | ~\totalcoeffs[1] )));
  assign new_n45 = new_n46 & ~\totalcoeffs[0]  & ~\totalcoeffs[2] ;
  assign new_n46 = (\ctable[1]  | ~\trailingones[1] ) & ((\totalcoeffs[4]  & \trailingones[1] ) | (~\ctable[1]  & (\trailingones[0]  ? \totalcoeffs[4]  : \ctable[0] )));
  assign new_n47 = (~\ctable[0]  | ((~\totalcoeffs[2]  | ~\trailingones[1]  | ~\totalcoeffs[0]  | ~\trailingones[0] ) & (\totalcoeffs[2]  | \trailingones[0]  | \totalcoeffs[0]  | \trailingones[1] ))) & (~\totalcoeffs[2]  | ((\totalcoeffs[0]  | ((\ctable[0]  | ~\trailingones[0] ) & (~\ctable[1]  | ~\trailingones[1] ))) & (~\trailingones[0]  | ~\ctable[1]  | ~\trailingones[1] ) & (\trailingones[0]  | \ctable[0]  | \trailingones[1] )));
  assign new_n48 = (\totalcoeffs[2]  | ((~\totalcoeffs[0]  | ((\ctable[0]  | ~\trailingones[0] ) & (~\ctable[1]  | ~\trailingones[1] )) | (\ctable[1]  & \trailingones[0] )) & (\totalcoeffs[0]  | ~\ctable[0]  | \trailingones[0]  | ~\trailingones[1] ))) & (\trailingones[1]  | ((\ctable[1]  | \trailingones[0] ) & (\ctable[0]  | ~\trailingones[0]  | (~\totalcoeffs[2]  & (\totalcoeffs[0]  | ~\ctable[1] )))));
  assign new_n49 = ~\ctable[1]  & \trailingones[0]  & \totalcoeffs[2]  & ~\trailingones[1] ;
  assign new_n50 = \trailingones[1]  ? (~new_n55 & (new_n54 | ~\ctable[1] )) : (~new_n51 & (new_n53 | \ctable[1] ));
  assign new_n51 = ~new_n52 & ~\ctable[0] ;
  assign new_n52 = (~\totalcoeffs[3]  | (~\totalcoeffs[1]  & ~\trailingones[0] ) | (\totalcoeffs[2]  & (\totalcoeffs[1]  | ~\ctable[1] ))) & (\totalcoeffs[2]  | (~\totalcoeffs[0]  & \trailingones[0] ) | (~\totalcoeffs[1]  & ~\ctable[1] ) | (\totalcoeffs[0]  & \totalcoeffs[1] ));
  assign new_n53 = (\totalcoeffs[1]  | ((~\ctable[0]  | \totalcoeffs[2]  | \trailingones[0] ) & (~\totalcoeffs[0]  | (~\ctable[0]  & \trailingones[0] ) | (~\totalcoeffs[3]  & ~\trailingones[0] )))) & (\totalcoeffs[0]  | (~\totalcoeffs[2]  & (~\totalcoeffs[3]  | ~\trailingones[0] ))) & (~\totalcoeffs[1]  | ((~\totalcoeffs[0]  | \totalcoeffs[2]  | ~\trailingones[0] ) & (\totalcoeffs[0]  | ~\ctable[0] ) & (~\totalcoeffs[0]  | \ctable[0] )));
  assign new_n54 = (~\totalcoeffs[0]  | ((~\totalcoeffs[1]  | ~\ctable[0] ) & (\totalcoeffs[1]  | \totalcoeffs[2]  | ~\totalcoeffs[3]  | \trailingones[0] ))) & (~\ctable[0]  | (~\totalcoeffs[2]  & ~\totalcoeffs[3] )) & (~\totalcoeffs[3]  | ~\totalcoeffs[1]  | ~\totalcoeffs[2] );
  assign new_n55 = \ctable[0]  & ~\trailingones[0]  & \totalcoeffs[3]  & \totalcoeffs[1]  & \totalcoeffs[2] ;
  assign new_n56 = new_n26 & ~\ctable[1]  & ~new_n57 & ~\totalcoeffs[4] ;
  assign new_n57 = (~\totalcoeffs[2]  | \trailingones[1]  | \totalcoeffs[0]  | \totalcoeffs[1] ) & (~\ctable[2]  | ((\trailingones[0]  | (\totalcoeffs[2]  & (\totalcoeffs[0]  | \totalcoeffs[1] )) | (~\totalcoeffs[2]  & (~\totalcoeffs[0]  | \trailingones[1] ) & (~\totalcoeffs[0]  | ~\totalcoeffs[1] ))) & (~\totalcoeffs[1]  | \trailingones[1]  | \totalcoeffs[2]  | ~\trailingones[0] )));
  assign \coeff_token[2]  = (~new_n59 & ~\ctable[2] ) | (new_n72 & new_n73 & ~\ctable[0]  & ~\ctable[1] );
  assign new_n59 = (\totalcoeffs[0]  | (~new_n65 & (new_n60 | \totalcoeffs[4] ))) & (\totalcoeffs[4]  | (~new_n67 & ~new_n70));
  assign new_n60 = ~new_n61 & (new_n62 | \trailingones[1] ) & ~new_n63 & new_n64;
  assign new_n61 = ~\ctable[1]  & (\totalcoeffs[3]  | ~\trailingones[1] ) & ((\totalcoeffs[1]  & ~\totalcoeffs[3] ) | (~\trailingones[0]  & \trailingones[1] ));
  assign new_n62 = (~\totalcoeffs[1]  | ((~\ctable[0]  | (\totalcoeffs[3]  & (\totalcoeffs[2]  | ~\trailingones[0] ))) & (~\trailingones[0]  | (\totalcoeffs[3]  & (~\totalcoeffs[2]  | \ctable[0] ))))) & (\totalcoeffs[1]  | \totalcoeffs[3]  | \trailingones[0]  | \ctable[0]  | ~\ctable[1] );
  assign new_n63 = \ctable[0]  & ((~\totalcoeffs[1]  & (\totalcoeffs[2]  | (\totalcoeffs[3]  & \trailingones[0] ))) | (\totalcoeffs[2]  & (\ctable[1]  | ~\trailingones[0] )));
  assign new_n64 = (~\totalcoeffs[3]  | ((\totalcoeffs[2]  | (~\trailingones[1]  & (~\ctable[1]  | (\totalcoeffs[1]  & \trailingones[0] )))) & (\totalcoeffs[1]  | \trailingones[0]  | ~\trailingones[1] ))) & (~\totalcoeffs[1]  | \totalcoeffs[3]  | ~\trailingones[1]  | ~\ctable[1]  | \trailingones[0] );
  assign new_n65 = ~\totalcoeffs[2]  & ~new_n66 & ~\totalcoeffs[1]  & ~\totalcoeffs[3]  & \totalcoeffs[4] ;
  assign new_n66 = ~\ctable[0]  & (~\trailingones[0]  | \trailingones[1] ) & (\ctable[1]  | \trailingones[0] );
  assign new_n67 = ~\ctable[1]  & ((~new_n68 & ~\totalcoeffs[3] ) | (~new_n69 & \totalcoeffs[0] ));
  assign new_n68 = (~\totalcoeffs[1]  | (\ctable[0]  ? (\trailingones[0]  ? (~\totalcoeffs[0]  | ~\trailingones[1] ) : \trailingones[1] ) : (~\totalcoeffs[2]  | ~\trailingones[1] ))) & (~\totalcoeffs[2]  | ((\totalcoeffs[1]  | \trailingones[1] ) & (~\trailingones[0]  | \trailingones[1] ) & (\trailingones[0]  | ~\trailingones[1] )));
  assign new_n69 = ((\ctable[0]  & (\totalcoeffs[1]  | ~\totalcoeffs[3] )) | \totalcoeffs[2]  | \trailingones[0]  | \trailingones[1] ) & (\totalcoeffs[1]  | ~\trailingones[0]  | ~\trailingones[1]  | (~\totalcoeffs[2]  & ~\totalcoeffs[3] ));
  assign new_n70 = ~new_n71 & \totalcoeffs[0]  & ~\ctable[0] ;
  assign new_n71 = (~\totalcoeffs[3]  | ((~\ctable[1]  | ((\totalcoeffs[1]  | ~\trailingones[0] ) & (~\totalcoeffs[2]  | ~\totalcoeffs[1]  | ~\trailingones[1] ))) & (~\trailingones[0]  | (\totalcoeffs[2]  & ~\trailingones[1] )) & (~\totalcoeffs[2]  | \trailingones[0]  | \trailingones[1] ))) & (\totalcoeffs[2]  | ((~\trailingones[0]  | ((~\totalcoeffs[1]  | \trailingones[1] ) & (~\ctable[1]  | (~\totalcoeffs[1]  & \trailingones[1] )))) & (\totalcoeffs[3]  | \trailingones[0]  | (~\totalcoeffs[1]  ^ ~\trailingones[1] ))));
  assign new_n72 = ~\totalcoeffs[2]  & ~\totalcoeffs[3] ;
  assign new_n73 = ~\totalcoeffs[4]  & ((\totalcoeffs[1]  & ((~\totalcoeffs[0]  & ~\trailingones[1] ) | (\totalcoeffs[0]  & \trailingones[1]  & \ctable[2]  & \trailingones[0] ))) | (~\totalcoeffs[1]  & ~\trailingones[0]  & \totalcoeffs[0]  & ~\trailingones[1] ));
  assign \coeff_token[3]  = ~\ctable[2]  & ((~new_n75 & ~\totalcoeffs[0] ) | (~\totalcoeffs[4]  & (~new_n83 | (~new_n80 & \totalcoeffs[0] ))));
  assign new_n75 = (new_n76 | \totalcoeffs[1] ) & (\totalcoeffs[4]  | ((~new_n78 | ~\totalcoeffs[2] ) & (new_n79 | ~\totalcoeffs[1] )));
  assign new_n76 = (\totalcoeffs[4]  | ((~\totalcoeffs[2]  | ~\ctable[1] ) & (~\totalcoeffs[3]  | (~\ctable[1]  & \trailingones[1] )))) & (new_n77 | \totalcoeffs[2]  | \totalcoeffs[3]  | ~\totalcoeffs[4] );
  assign new_n77 = (~\ctable[0]  | ~\ctable[1] ) & (\ctable[0]  | \ctable[1]  | ~\trailingones[0]  | ~\trailingones[1] );
  assign new_n78 = ~\ctable[0]  & \ctable[1] ;
  assign new_n79 = (~\totalcoeffs[2]  | ((\trailingones[0]  | \ctable[0]  | \trailingones[1] ) & (\ctable[1]  | ~\trailingones[1]  | ~\ctable[0]  | ~\trailingones[0] ))) & (\trailingones[0]  | \ctable[0]  | ~\ctable[1] ) & (~\totalcoeffs[3]  | (\ctable[0]  & (\ctable[1]  | ~\trailingones[0] )));
  assign new_n80 = (new_n82 | \trailingones[1] ) & (new_n81 | ~\totalcoeffs[1] );
  assign new_n81 = (\trailingones[1]  | (\totalcoeffs[2]  ? (\trailingones[0]  ? \ctable[0]  : \totalcoeffs[3] ) : (~\ctable[0]  | ~\trailingones[0] ))) & (\totalcoeffs[2]  | (~\totalcoeffs[3]  & (\trailingones[0]  | ~\ctable[0]  | ~\trailingones[1] ))) & ((\totalcoeffs[3]  & ~\ctable[0] ) | (~\ctable[1]  & (~\totalcoeffs[3]  | \trailingones[0] )));
  assign new_n82 = (\totalcoeffs[3]  | \ctable[0]  | ~\ctable[1] ) & (\totalcoeffs[1]  | \ctable[1]  | ~\ctable[0]  | \totalcoeffs[2]  | \trailingones[0] );
  assign new_n83 = (new_n85 | \totalcoeffs[1] ) & (new_n86 | ~\totalcoeffs[3] ) & (~new_n78 | ~new_n84 | ~\totalcoeffs[1] );
  assign new_n84 = \trailingones[0]  & ~\trailingones[1] ;
  assign new_n85 = (~\trailingones[1]  | (((~\totalcoeffs[2]  & ~\totalcoeffs[3] ) | \ctable[0]  | ~\ctable[1] ) & (~\totalcoeffs[2]  | ~\totalcoeffs[3]  | \ctable[1] ))) & (\trailingones[0]  | ((~\totalcoeffs[3]  | \ctable[1] ) & (\trailingones[1]  | \ctable[0]  | ~\ctable[1] )));
  assign new_n86 = (\totalcoeffs[2]  | ((\ctable[0]  | \trailingones[1] ) & (\ctable[1]  | \trailingones[0] ))) & (\ctable[1]  | (\ctable[0]  & \trailingones[1] ) | ((~\totalcoeffs[1]  | ~\trailingones[1] ) & (~\ctable[0]  | ~\trailingones[0] )));
  assign \coeff_token[4]  = new_n88 & \ctable[0]  & \ctable[1] ;
  assign new_n88 = ~\ctable[2]  & ((~\totalcoeffs[4]  & \totalcoeffs[2]  & (\totalcoeffs[0]  | \totalcoeffs[1] )) | ((~\totalcoeffs[3]  ^ ~\totalcoeffs[4] ) & ~\totalcoeffs[0]  & ~\totalcoeffs[1]  & ~\totalcoeffs[2] ));
  assign \coeff_token[5]  = new_n90 & \ctable[0]  & \ctable[1] ;
  assign new_n90 = ~\ctable[2]  & ((~\totalcoeffs[0]  & ~\totalcoeffs[1]  & ~\totalcoeffs[2]  & ~\totalcoeffs[3]  & \totalcoeffs[4] ) | (\totalcoeffs[3]  & ~\totalcoeffs[4]  & (\totalcoeffs[0]  | \totalcoeffs[1]  | \totalcoeffs[2] )));
  assign \ctoken_len[0]  = (new_n104 | \ctable[2] ) & (\totalcoeffs[4]  | (~new_n92 & (new_n97 | \ctable[2] )));
  assign new_n92 = ~\totalcoeffs[3]  & ((~new_n93 & ~\ctable[2] ) | (~new_n96 & ~\ctable[0]  & ~\ctable[1] ));
  assign new_n93 = ~new_n95 & (new_n94 | ~\trailingones[0] ) & (~\ctable[1]  | (~\trailingones[0]  ^ ~\trailingones[1] ) | (~\totalcoeffs[2]  & \trailingones[1] ) | (\totalcoeffs[2]  & ~\trailingones[1] ));
  assign new_n94 = (~\totalcoeffs[0]  | ((\ctable[1]  | \trailingones[1]  | (\totalcoeffs[2]  & (\totalcoeffs[1]  | \ctable[0] ))) & (~\ctable[0]  | ~\totalcoeffs[1]  | ~\trailingones[1] ))) & (~\totalcoeffs[2]  | \totalcoeffs[0]  | (~\ctable[0]  & ~\trailingones[1] ));
  assign new_n95 = \ctable[0]  & ((~\trailingones[0]  & ~\totalcoeffs[1]  & ~\trailingones[1] ) | (\totalcoeffs[2]  & ~\totalcoeffs[0]  & \trailingones[1] ));
  assign new_n96 = (\totalcoeffs[0]  | ((\totalcoeffs[2]  | ~\totalcoeffs[1]  | \trailingones[1] ) & (\totalcoeffs[1]  | ~\totalcoeffs[2]  | (\trailingones[0]  & (~\ctable[2]  | \trailingones[1] ))))) & (\totalcoeffs[2]  | ~\ctable[2]  | ((\trailingones[0]  | \trailingones[1] ) & (~\totalcoeffs[1]  | ~\trailingones[1]  | ~\totalcoeffs[0]  | ~\trailingones[0] )));
  assign new_n97 = (new_n98 | ~\totalcoeffs[3] ) & (new_n103 | \totalcoeffs[1] ) & (new_n102 | ~\totalcoeffs[1] );
  assign new_n98 = (new_n100 | \ctable[0] ) & ~new_n101 & (~new_n99 | (~\totalcoeffs[0]  & ~\trailingones[0] ) | (\totalcoeffs[0]  & ~\ctable[0] ));
  assign new_n99 = \totalcoeffs[1]  & ~\trailingones[1] ;
  assign new_n100 = (~\totalcoeffs[0]  | ((~\totalcoeffs[1]  | ~\trailingones[1] ) & (\trailingones[0]  | \totalcoeffs[1]  | \trailingones[1] ))) & (~\totalcoeffs[2]  | ~\totalcoeffs[1]  | \trailingones[1] ) & (\totalcoeffs[0]  | \ctable[1]  | (~\totalcoeffs[1]  & ~\trailingones[0] ) | (~\totalcoeffs[1]  & ~\trailingones[1] ) | (\totalcoeffs[1]  & \trailingones[1] ));
  assign new_n101 = \trailingones[1]  & ((~\trailingones[0]  & \totalcoeffs[0]  & \totalcoeffs[1] ) | (~\totalcoeffs[2]  & ((\totalcoeffs[1]  & ~\trailingones[0] ) | (\totalcoeffs[0]  & \ctable[1]  & \trailingones[0] ))));
  assign new_n102 = (\trailingones[0]  | (((~\totalcoeffs[0]  ^ ~\trailingones[1] ) | \totalcoeffs[2]  | ~\ctable[0] ) & (\totalcoeffs[0]  | ~\trailingones[1]  | (~\ctable[1]  & (~\totalcoeffs[2]  | \ctable[0] ))))) & (~\ctable[1]  | ((~\trailingones[0]  | ~\totalcoeffs[0]  | ~\trailingones[1] ) & (~\totalcoeffs[2]  | (~\trailingones[0]  & ~\trailingones[1] ))));
  assign new_n103 = (\totalcoeffs[2]  | \trailingones[1]  | ((~\totalcoeffs[0]  | \ctable[0] ) & (\trailingones[0]  | (~\totalcoeffs[0]  & ~\ctable[1] )))) & (\totalcoeffs[0]  | ~\trailingones[1]  | ~\totalcoeffs[2]  | ~\trailingones[0] );
  assign new_n104 = (~\ctable[0]  | ~\ctable[1] ) & (\totalcoeffs[1]  | ~\totalcoeffs[4]  | ~new_n72 | \totalcoeffs[0] );
  assign \ctoken_len[1]  = ~new_n116 & (\totalcoeffs[4]  | ((new_n106 | \ctable[2] ) & (~new_n114 | ~new_n26)));
  assign new_n106 = ~new_n107 & (\ctable[0]  | ((new_n111 | ~\totalcoeffs[1] ) & ~new_n113 & (new_n112 | \totalcoeffs[1] )));
  assign new_n107 = ~\ctable[1]  & ((~new_n108 & \totalcoeffs[1] ) | new_n110 | (~new_n109 & ~\totalcoeffs[1] ));
  assign new_n108 = (\totalcoeffs[3]  | ((~\trailingones[0]  | (\totalcoeffs[0]  ? (~\totalcoeffs[2]  ^ \trailingones[1] ) : (~\ctable[0]  | \trailingones[1] ))) & (~\totalcoeffs[2]  | ~\trailingones[1]  | ~\ctable[0]  | \trailingones[0] ))) & (\trailingones[0]  | ((\totalcoeffs[0]  | ((~\totalcoeffs[2]  | \trailingones[1] ) & (~\totalcoeffs[3]  | ~\ctable[0] ))) & (~\totalcoeffs[3]  | \totalcoeffs[2]  | ~\ctable[0] ))) & (\totalcoeffs[2]  | ~\totalcoeffs[3]  | ~\ctable[0]  | \trailingones[1] );
  assign new_n109 = (~\ctable[0]  | ((~\totalcoeffs[2]  | ~\totalcoeffs[3] ) & ((~\trailingones[0]  ^ ~\trailingones[1] ) | (~\totalcoeffs[2]  & (~\totalcoeffs[0]  | ~\totalcoeffs[3] ))))) & (~\totalcoeffs[2]  | \trailingones[0]  | \trailingones[1]  | ~\totalcoeffs[0]  | ~\totalcoeffs[3] );
  assign new_n110 = \totalcoeffs[2]  & \totalcoeffs[3]  & \trailingones[0]  & \ctable[0]  & \trailingones[1] ;
  assign new_n111 = (\totalcoeffs[2]  | ((~\totalcoeffs[0]  | ((\totalcoeffs[3]  | ~\trailingones[0] ) & (~\ctable[1]  | ~\trailingones[1] ))) & (~\trailingones[1]  | ((~\ctable[1]  | \trailingones[0] ) & (~\totalcoeffs[3]  | \ctable[1]  | \totalcoeffs[0]  | ~\trailingones[0] ))) & (\trailingones[1]  | ~\ctable[1]  | ~\trailingones[0] ))) & (~\totalcoeffs[2]  | ((\totalcoeffs[3]  | ~\trailingones[0]  | ~\trailingones[1] ) & (\ctable[1]  | ((~\totalcoeffs[0]  | ~\trailingones[0] ) & (~\totalcoeffs[3]  | (\trailingones[0]  & \trailingones[1] )))))) & (\totalcoeffs[3]  | \ctable[1]  | \trailingones[0]  | \trailingones[1] );
  assign new_n112 = (~\totalcoeffs[3]  | ((\totalcoeffs[2]  | \ctable[1]  | (\trailingones[1]  ? \trailingones[0]  : \totalcoeffs[0] )) & (\totalcoeffs[0]  | ~\ctable[1]  | (~\trailingones[0]  ^ ~\trailingones[1] )))) & (\trailingones[1]  | (\totalcoeffs[0]  ? (~\ctable[1]  | ~\trailingones[0] ) : (\totalcoeffs[2]  | \trailingones[0] ))) & (~\totalcoeffs[2]  | (~\trailingones[0]  & ~\trailingones[1] ) | (~\ctable[1]  & (\totalcoeffs[3]  | \trailingones[0] )));
  assign new_n113 = ~\totalcoeffs[2]  & \totalcoeffs[3]  & ~\trailingones[1]  & \ctable[1]  & ~\trailingones[0] ;
  assign new_n114 = ~new_n115 & ~\totalcoeffs[1]  & ~\ctable[1] ;
  assign new_n115 = (\totalcoeffs[0]  | ~\totalcoeffs[2]  | (\trailingones[0]  ^ ~\trailingones[1] )) & (~\totalcoeffs[0]  | \totalcoeffs[2]  | \trailingones[1]  | ~\ctable[2]  | ~\trailingones[0] );
  assign new_n116 = new_n117 & ~\totalcoeffs[2]  & ~\ctable[0]  & ~\totalcoeffs[0]  & ~\totalcoeffs[3] ;
  assign new_n117 = \totalcoeffs[4]  & ~\ctable[2]  & ~\totalcoeffs[1]  & ~\ctable[1] ;
  assign \ctoken_len[2]  = (new_n119 | \totalcoeffs[4] ) & (~new_n127 | ~new_n26 | ~\totalcoeffs[4]  | \ctable[2] );
  assign new_n119 = ~new_n124 & (\ctable[2]  | (~new_n120 & (new_n126 | ~\totalcoeffs[3]  | \ctable[0] )));
  assign new_n120 = ~\ctable[1]  & ((~new_n121 & ~\trailingones[0] ) | (~new_n122 & ~\trailingones[1] ) | (~new_n123 & \trailingones[1] ));
  assign new_n121 = (\totalcoeffs[0]  | ((~\totalcoeffs[1]  | \totalcoeffs[3]  | ~\trailingones[1] ) & (~\ctable[0]  | \trailingones[1]  | \totalcoeffs[1]  | \totalcoeffs[2] ))) & (\totalcoeffs[2]  | (\ctable[0]  ? \totalcoeffs[1]  : \totalcoeffs[3] ) | (~\totalcoeffs[1]  & ~\trailingones[1] ) | (~\totalcoeffs[3]  & \trailingones[1] )) & (~\totalcoeffs[2]  | \totalcoeffs[3]  | ((\totalcoeffs[1]  | \ctable[0] ) & (\totalcoeffs[1]  | \trailingones[1] ) & (~\totalcoeffs[1]  | ~\trailingones[1] )));
  assign new_n122 = (~\trailingones[0]  | ((\totalcoeffs[2]  | (~\totalcoeffs[0]  & ~\totalcoeffs[3] ) | (\ctable[0]  ? \totalcoeffs[1]  : \totalcoeffs[3] )) & (\totalcoeffs[1]  | \totalcoeffs[3]  | ~\totalcoeffs[0]  | \ctable[0] ))) & (~\totalcoeffs[1]  | ~\totalcoeffs[2]  | (\totalcoeffs[3]  & \ctable[0] ) | (~\ctable[0]  & (\totalcoeffs[0]  | ~\trailingones[0] )));
  assign new_n123 = (~\totalcoeffs[1]  | (\totalcoeffs[3]  & (\totalcoeffs[2]  | ~\trailingones[0] )) | (\ctable[0]  ? ~\totalcoeffs[3]  : ~\totalcoeffs[2] )) & (\totalcoeffs[2]  | ~\trailingones[0]  | \totalcoeffs[1]  | ~\totalcoeffs[3]  | (~\totalcoeffs[0]  & \ctable[0] ));
  assign new_n124 = ~\ctable[1]  & new_n26 & ~new_n125;
  assign new_n125 = (\totalcoeffs[1]  | ((\trailingones[1]  | (\totalcoeffs[0]  ? (\totalcoeffs[2]  | ~\trailingones[0] ) : (~\totalcoeffs[2]  ^ ~\trailingones[0] ))) & (\totalcoeffs[0]  | \trailingones[0]  | ~\totalcoeffs[2]  | ~\trailingones[1] ))) & (\totalcoeffs[0]  | \totalcoeffs[2]  | \trailingones[0]  | ~\totalcoeffs[1]  | ~\trailingones[1] );
  assign new_n126 = (~\totalcoeffs[0]  | ((~\ctable[1]  | (~\totalcoeffs[1]  & \trailingones[1] )) & (~\totalcoeffs[2]  | (~\totalcoeffs[1]  & (\trailingones[0]  | \trailingones[1] ))))) & (~\totalcoeffs[2]  | (~\ctable[1]  & (~\totalcoeffs[1]  | \trailingones[0] ))) & ((\trailingones[0]  & (~\totalcoeffs[1]  | \trailingones[1] )) | ~\ctable[1]  | (~\totalcoeffs[1]  & \trailingones[1] ));
  assign new_n127 = ~\totalcoeffs[0]  & ~\totalcoeffs[1]  & ~\totalcoeffs[2] ;
  assign \ctoken_len[3]  = new_n129 & ~new_n140 & (~\totalcoeffs[4]  | (~\totalcoeffs[2]  & ~\totalcoeffs[1]  & ~\totalcoeffs[3] ));
  assign new_n129 = ~new_n137 & ~new_n138 & (new_n130 | ~\totalcoeffs[1] ) & ~new_n139 & (new_n133 | \totalcoeffs[1] );
  assign new_n130 = new_n132 & (new_n131 | \totalcoeffs[2] );
  assign new_n131 = (\totalcoeffs[3]  | (~\ctable[0]  & ~\trailingones[1] )) & (\totalcoeffs[0]  | ~\trailingones[0]  | (\totalcoeffs[3]  & (~\ctable[1]  | ~\trailingones[1] )));
  assign new_n132 = (\ctable[0]  | ~\totalcoeffs[2]  | ~\totalcoeffs[3]  | \ctable[1]  | (\trailingones[0]  & \trailingones[1] )) & (\totalcoeffs[3]  | (~\ctable[1]  & (~\trailingones[0]  | ~\ctable[0]  | ~\trailingones[1] )));
  assign new_n133 = \totalcoeffs[2]  ? ~new_n136 : (\totalcoeffs[3]  ? new_n134 : new_n135);
  assign new_n134 = (~\ctable[1]  | ~\trailingones[1] ) & (\totalcoeffs[0]  | ~\trailingones[0]  | (~\ctable[1]  & (~\ctable[0]  | ~\trailingones[1] )));
  assign new_n135 = \totalcoeffs[4]  & (\ctable[0]  | \ctable[1] );
  assign new_n136 = ~\totalcoeffs[3]  & (\ctable[0]  | \trailingones[1] ) & (\trailingones[0]  | (\ctable[0]  & \trailingones[1] ));
  assign new_n137 = \totalcoeffs[0]  & (\ctable[2]  | (~\totalcoeffs[2]  & ~\totalcoeffs[1]  & ~\totalcoeffs[3] ));
  assign new_n138 = \ctable[2]  & (~\totalcoeffs[2]  | \totalcoeffs[3] );
  assign new_n139 = \ctable[1]  & (\ctable[0]  | (\totalcoeffs[2]  & ~\totalcoeffs[3] ));
  assign new_n140 = ~new_n141 & (\totalcoeffs[1]  | (~\trailingones[0]  & ~\trailingones[1] ));
  assign new_n141 = ~\ctable[2]  & (~\totalcoeffs[0]  | \ctable[0]  | ~\totalcoeffs[2]  | ~\totalcoeffs[3]  | \ctable[1] );
  assign \ctoken_len[4]  = ~new_n143 & ~\ctable[2]  & ~\ctable[0]  & ~\ctable[1] ;
  assign new_n143 = (\totalcoeffs[0]  | \totalcoeffs[1]  | \totalcoeffs[2]  | \totalcoeffs[3]  | ~\totalcoeffs[4] ) & (~\totalcoeffs[2]  | new_n144 | ~\totalcoeffs[3]  | \totalcoeffs[4] );
  assign new_n144 = (~\totalcoeffs[1]  | (~\totalcoeffs[0]  & \trailingones[1] )) & (\trailingones[0]  | (~\totalcoeffs[1]  & (~\totalcoeffs[0]  | \trailingones[1] )));
endmodule


