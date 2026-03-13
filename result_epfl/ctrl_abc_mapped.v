// Benchmark "/Users/yoyowang/Desktop/benchmark/ctrl" written by ABC on Fri Mar 13 13:03:15 2026

module \/Users/yoyowang/Desktop/benchmark/ctrl  ( 
    \opcode[0] , \opcode[1] , \opcode[2] , \opcode[3] , \opcode[4] ,
    \op_ext[0] , \op_ext[1] ,
    \sel_reg_dst[0] , \sel_reg_dst[1] , \sel_alu_opB[0] , \sel_alu_opB[1] ,
    \alu_op[0] , \alu_op[1] , \alu_op[2] , \alu_op_ext[0] ,
    \alu_op_ext[1] , \alu_op_ext[2] , \alu_op_ext[3] , halt, reg_write,
    sel_pc_opA, sel_pc_opB, beqz, bnez, bgez, bltz, jump, Cin, invA, invB,
    sign, mem_write, sel_wb  );
  input  \opcode[0] , \opcode[1] , \opcode[2] , \opcode[3] , \opcode[4] ,
    \op_ext[0] , \op_ext[1] ;
  output \sel_reg_dst[0] , \sel_reg_dst[1] , \sel_alu_opB[0] ,
    \sel_alu_opB[1] , \alu_op[0] , \alu_op[1] , \alu_op[2] ,
    \alu_op_ext[0] , \alu_op_ext[1] , \alu_op_ext[2] , \alu_op_ext[3] ,
    halt, reg_write, sel_pc_opA, sel_pc_opB, beqz, bnez, bgez, bltz, jump,
    Cin, invA, invB, sign, mem_write, sel_wb;
  wire new_n40, new_n58, new_n60;
  assign \sel_reg_dst[0]  = \opcode[2]  ? ((\opcode[4]  & ~\opcode[1]  & \opcode[3] ) | (\opcode[1]  & (~\opcode[3]  ^ \opcode[4] ))) : ((\opcode[0]  & ~\opcode[1]  & \opcode[3]  & \opcode[4] ) | (\opcode[4]  & \opcode[1]  & \opcode[3] ));
  assign \sel_reg_dst[1]  = \opcode[2]  ? (\opcode[1]  & (~\opcode[1]  | (~\opcode[3]  & (\opcode[3]  | ~\opcode[4] )))) : (\opcode[1]  ? (~\opcode[3]  & (\opcode[3]  | \opcode[4] )) : (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & \opcode[4] ))));
  assign \sel_alu_opB[0]  = ~\opcode[2]  & (\opcode[2]  | (\opcode[1]  ? (\opcode[0]  ? (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] )) : (\opcode[3]  ^ \opcode[4] )) : (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & \opcode[4] )))));
  assign \sel_alu_opB[1]  = \opcode[2]  ? (~\opcode[3]  & (\opcode[3]  | \opcode[4] )) : (\opcode[1]  ? (~\opcode[3]  & (\opcode[3]  | \opcode[4] )) : (((~\opcode[3]  & \opcode[4] ) | \opcode[0]  | \opcode[3] ) & (~\opcode[0]  | (\opcode[3]  ^ \opcode[4] ))));
  assign \alu_op[0]  = \opcode[2]  ? (\opcode[0]  & (~\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] )))) : new_n40;
  assign new_n40 = \opcode[1]  & ((\opcode[4]  & \op_ext[0]  & ~\opcode[0]  & \opcode[3] ) | (\opcode[0]  & (((~\opcode[3]  | ~\opcode[4] ) & \opcode[3]  & ~\op_ext[1] ) | (\op_ext[1]  & (((~\opcode[3]  | ~\opcode[4] ) & \opcode[3]  & ~\op_ext[0] ) | (\opcode[3]  & \op_ext[0] ))))));
  assign \alu_op[1]  = \opcode[2]  ? (\opcode[1]  & (~\opcode[1]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] )))) : (\opcode[1]  & (((~\opcode[3]  | ~\opcode[4] ) & \opcode[3]  & ~\op_ext[1] ) | (\opcode[3]  & \op_ext[1] )));
  assign \alu_op[2]  = (~\opcode[2]  & (((\opcode[3]  | \opcode[4] ) & ~\opcode[1]  & (~\opcode[3]  | ~\opcode[4] )) | (\opcode[1]  & (((\opcode[3]  | \opcode[4] ) & ~\opcode[0]  & (~\opcode[3]  | ~\opcode[4] )) | (\opcode[0]  & (\opcode[3]  | (~\opcode[3]  & \opcode[4] ))))))) | (\opcode[4]  & \opcode[2]  & \opcode[3] );
  assign \alu_op_ext[0]  = ((\opcode[1]  | (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & \opcode[4] )))) & ~\opcode[1]  & ~\opcode[2] ) | (\opcode[2]  & ((\opcode[0]  & ~\opcode[1]  & \opcode[3]  & \opcode[4] ) | (\opcode[1]  & (((\opcode[3]  | ~\opcode[4] ) & ~\opcode[0]  & ~\opcode[3] ) | (\opcode[0]  & (~\opcode[3]  ^ \opcode[4] ))))));
  assign \alu_op_ext[1]  = ((~\opcode[1]  | (~\opcode[0]  & (\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] ))))) & \opcode[1]  & ~\opcode[2] ) | ((~\opcode[3]  ^ \opcode[4] ) & \opcode[1]  & \opcode[2] );
  assign \alu_op_ext[2]  = \opcode[2]  ? (\opcode[1]  & (~\opcode[1]  | (~\opcode[3]  & (\opcode[3]  | ~\opcode[4] )))) : (\opcode[1]  ? (~\opcode[0]  & (\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] )))) : (\opcode[3]  & \opcode[4] ));
  assign \alu_op_ext[3]  = (~\opcode[2]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] ))) & ((\opcode[1]  & (\opcode[0]  ? (\opcode[3]  | (~\opcode[3]  & \opcode[4] )) : \opcode[3] )) | \opcode[2]  | ((\opcode[3]  | \opcode[4] ) & ~\opcode[1]  & (~\opcode[3]  | ~\opcode[4] )));
  assign halt = ~\opcode[2]  & (\opcode[2]  | (~\opcode[1]  & (\opcode[1]  | (~\opcode[0]  & (\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | ~\opcode[4] )))))));
  assign reg_write = \opcode[2]  ? (\opcode[1]  ? (~\opcode[3]  | (\opcode[3]  & \opcode[4] )) : \opcode[4] ) : (\opcode[1]  ? (\opcode[3]  | (~\opcode[3]  & \opcode[4] )) : (\opcode[0]  ? (\opcode[3]  | (~\opcode[3]  & \opcode[4] )) : \opcode[3] ));
  assign sel_pc_opA = \opcode[2]  & (~\opcode[2]  | (\opcode[0]  & (~\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | ~\opcode[4] )))));
  assign sel_pc_opB = \opcode[2]  & (~\opcode[2]  | (~\opcode[0]  & (\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | ~\opcode[4] )))));
  assign beqz = \opcode[2]  & (~\opcode[2]  | (~\opcode[1]  & (\opcode[1]  | (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] )))))));
  assign bnez = \opcode[2]  & (~\opcode[2]  | (~\opcode[1]  & (\opcode[1]  | (\opcode[0]  & (~\opcode[0]  | (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] )))))));
  assign bgez = \opcode[2]  & (~\opcode[2]  | (\opcode[1]  & (~\opcode[1]  | (\opcode[0]  & (~\opcode[0]  | (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] )))))));
  assign bltz = \opcode[2]  & (~\opcode[2]  | (\opcode[1]  & (~\opcode[1]  | (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] )))))));
  assign jump = \opcode[2]  & (~\opcode[2]  | (~\opcode[3]  & (\opcode[3]  | ~\opcode[4] )));
  assign Cin = (((((~\opcode[3]  | ~\opcode[4] ) & \opcode[3]  & ~\op_ext[0] ) | (\opcode[3]  & \op_ext[0] )) & \opcode[0]  & \opcode[1] ) | \opcode[2]  | (\opcode[0]  & ~\opcode[1]  & (~\opcode[0]  | (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] ))))) & (~\opcode[2]  | (\opcode[1]  ? (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & \opcode[4] ))) : (\opcode[3]  & \opcode[4] )));
  assign invA = ~\opcode[2]  & (new_n58 | \opcode[2] );
  assign new_n58 = \opcode[1]  ? (\opcode[0]  & (~\opcode[0]  | (~\op_ext[1]  & (\op_ext[1]  | (\op_ext[0]  & \opcode[3]  & \opcode[4] ))))) : (\opcode[0]  & (~\opcode[0]  | (\opcode[3]  & (~\opcode[3]  | ~\opcode[4] ))));
  assign invB = \opcode[2]  ? (\opcode[1]  ? (~\opcode[0]  & (\opcode[0]  | (\opcode[3]  & \opcode[4] ))) : (\opcode[3]  & \opcode[4] )) : (~new_n60 & \opcode[0]  & \opcode[1] );
  assign new_n60 = ((\opcode[3]  & \opcode[4] ) | ~\opcode[3]  | \op_ext[1] ) & (~\op_ext[1]  | (((\opcode[3]  & \opcode[4] ) | ~\opcode[3]  | \op_ext[0] ) & (~\opcode[3]  | ~\op_ext[0] )));
  assign mem_write = ~\opcode[2]  & (\opcode[2]  | (\opcode[1]  ? (\opcode[0]  & (~\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] )))) : (~\opcode[0]  & (\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] ))))));
  assign sel_wb = ~\opcode[2]  & (\opcode[2]  | (~\opcode[1]  & (\opcode[1]  | (\opcode[0]  & (~\opcode[0]  | (~\opcode[3]  & (\opcode[3]  | \opcode[4] )))))));
  assign sign = 1'b1;
endmodule


