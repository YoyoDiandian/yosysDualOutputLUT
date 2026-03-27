// Yoyo FPGA Simulation Library
// LUT6D - Dual-output 6-input LUT

// LUT6D has two outputs:
// - O6: function of all 6 inputs (I0-I5)  
// - O5: function of 5 inputs (I0-I4), treating I5=0
// This is similar to Xilinx LUT6_2 but with different semantics

module GTP_LUT1
#(
    parameter [1:0] INIT = 2'h0
) (
    output wire Z,
    input wire I0
);
   $lut #(.WIDTH(1),.LUT(INIT)) lut1_cell(.A(I0),.Y(Z));
endmodule


module GTP_LUT2
#(
    parameter [3:0] INIT = 4'h0
) (
    output wire Z,
    input wire I0, I1
);
$lut #(.WIDTH(2),.LUT(INIT)) lut1_cell(.A({I1,I0}),.Y(Z));
endmodule



module GTP_LUT3
#(
    parameter [7:0] INIT = 8'h00
) (
    output wire Z,
    input wire I0, I1, I2
);
$lut #(.WIDTH(3),.LUT(INIT)) lut1_cell(.A({I2,I1,I0}),.Y(Z));
endmodule



module GTP_LUT4
#(
    parameter [15:0] INIT = 16'h0000
) (
    output wire Z,
    input wire I0, I1, I2, I3
);
$lut #(.WIDTH(4),.LUT(INIT)) lut1_cell(.A({I3,I2,I1,I0}),.Y(Z));
endmodule


module GTP_LUT5
#(
    parameter [31:0] INIT = 32'h0000_0000
) (
    output wire Z,
    input wire I0, I1, I2, I3, I4
);
$lut #(.WIDTH(5),.LUT(INIT)) lut1_cell(.A({I4,I3,I2,I1,I0}),.Y(Z));
endmodule

module GTP_LUT6
#(
    parameter [63:0] INIT = 64'h0000_0000_0000_0000
) (
    output wire Z,
    input wire I0, I1, I2, I3, I4, I5
);

$lut #(.WIDTH(6),.LUT(INIT)) lut1_cell(.A({I5,I4,I3,I2,I1,I0}),.Y(Z));
endmodule

module GTP_LUT6D(
    output Z,      // Primary output (6-input function)
    output Z5,      // Secondary output (5-input function, I5=0)
    input I0, I1, I2, I3, I4, I5
);
    parameter [63:0] INIT = 64'h0000000000000000;
    
    // O6: Full 6-input LUT function
    wire [31:0] s6_5 = I5 ? INIT[63:32] : INIT[31:0];
    wire [15:0] s6_4 = I4 ? s6_5[31:16] : s6_5[15:0];
    wire [ 7:0] s6_3 = I3 ? s6_4[15: 8] : s6_4[ 7:0];
    wire [ 3:0] s6_2 = I2 ? s6_3[ 7: 4] : s6_3[ 3:0];
    wire [ 1:0] s6_1 = I1 ? s6_2[ 3: 2] : s6_2[ 1:0];
    assign Z = I0 ? s6_1[1] : s6_1[0];
    
    // O5: 5-input LUT function (lower half of INIT, I5 treated as 0)
    wire [15:0] s5_4 = I4 ? INIT[31:16] : INIT[15:0];
    wire [ 7:0] s5_3 = I3 ? s5_4[15: 8] : s5_4[ 7:0];
    wire [ 3:0] s5_2 = I2 ? s5_3[ 7: 4] : s5_3[ 3:0];
    wire [ 1:0] s5_1 = I1 ? s5_2[ 3: 2] : s5_2[ 1:0];
    assign Z5 = I0 ? s5_1[1] : s5_1[0];
endmodule

module GTP_VERSAL_LUT6D(
    output O6,      // Public Versal-compatible dual-output LUT view
    output O5,      // O5 uses the lower 32 INIT bits with I5 fixed to 0
    input I0, I1, I2, I3, I4, I5
);
    parameter [63:0] INIT = 64'h0000000000000000;

    GTP_LUT6D #(
        .INIT(INIT)
    ) lut6d (
        .Z(O6),
        .Z5(O5),
        .I0(I0),
        .I1(I1),
        .I2(I2),
        .I3(I3),
        .I4(I4),
        .I5(I5)
    );
endmodule

module GTP_LUT5D(
    output Z,      // Primary output (5-input function)
    output Z5,     // Secondary output (4-input function, I4=0)
    input I0, I1, I2, I3, I4
);
    parameter [31:0] INIT = 32'h00000000;
    
    // O5: Full 5-input LUT function
    wire [15:0] s5_4 = I4 ? INIT[31:16] : INIT[15:0];
    wire [ 7:0] s5_3 = I3 ? s5_4[15: 8] : s5_4[ 7:0];
    wire [ 3:0] s5_2 = I2 ? s5_3[ 7: 4] : s5_3[ 3:0];
    wire [ 1:0] s5_1 = I1 ? s5_2[ 3: 2] : s5_2[ 1:0];
    assign Z = I0 ? s5_1[1] : s5_1[0];
    
    // O4: 4-input LUT function (lower half of INIT, I4 treated as 0)
    wire [ 7:0] s4_3 = I3 ? INIT[15: 8] : INIT[ 7:0];
    wire [ 3:0] s4_2 = I2 ? s4_3[ 7: 4] : s4_3[ 3:0];
    wire [ 1:0] s4_1 = I1 ? s4_2[ 3: 2] : s4_2[ 1:0];
    assign Z5 = I0 ? s4_1[1] : s4_1[0];
endmodule

module GTP_LUT4D(
    output Z,      // Primary output (4-input function)
    output Z5,     // Secondary output (3-input function, I3=0)
    input I0, I1, I2, I3
);
    parameter [15:0] INIT = 16'h0000;
    
    // O4: Full 4-input LUT function
    wire [ 7:0] s4_3 = I3 ? INIT[15: 8] : INIT[ 7:0];
    wire [ 3:0] s4_2 = I2 ? s4_3[ 7: 4] : s4_3[ 3:0];
    wire [ 1:0] s4_1 = I1 ? s4_2[ 3: 2] : s4_2[ 1:0];
    assign Z = I0 ? s4_1[1] : s4_1[0];
    
    // O3: 3-input LUT function (lower half of INIT, I3 treated as 0)
    wire [ 3:0] s3_2 = I2 ? INIT[ 7: 4] : INIT[ 3:0];
    wire [ 1:0] s3_1 = I1 ? s3_2[ 3: 2] : s3_2[ 1:0];
    assign Z5 = I0 ? s3_1[1] : s3_1[0];
endmodule
