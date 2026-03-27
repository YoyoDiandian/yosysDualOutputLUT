module GTP_LUT1
#(
    parameter [1:0] INIT = 2'h0
) (
    output Z,
    input I0
)  ;
endmodule


module GTP_LUT2
#(
    parameter [3:0] INIT = 4'h0
) (
    output Z,
    input I0,
    input I1
)  ;
endmodule


module GTP_LUT3
#(
    parameter [7:0] INIT = 8'h00
) (
    output Z,
    input I0,
    input I1,
    input I2
)  ;
endmodule


module GTP_LUT4
#(
    parameter [15:0] INIT = 16'h0000
) (
    output Z,
    input I0,
    input I1,
    input I2,
    input I3
)  ;
endmodule


module GTP_LUT5
#(
    parameter [31:0] INIT = 32'h00000000
) (
    output Z,
    input I0,
    input I1,
    input I2,
    input I3,
    input I4
)  ;
endmodule

module GTP_LUT6
#(
    parameter [63:0] INIT = 64'h0000000000000000
) (
    output Z,
    input I0,
    input I1,
    input I2,
    input I3,
    input I4,
    input I5
)  ;
endmodule

module GTP_LUT6D
#(
    parameter [63:0] INIT = 64'h0000000000000000
) (
    output Z,
    output Z5,
    input I0,
    input I1,
    input I2,
    input I3,
    input I4,
    input I5
)  ;
endmodule

module GTP_VERSAL_LUT6D
#(
    parameter [63:0] INIT = 64'h0000000000000000
) (
    output O6,
    output O5,
    input I0,
    input I1,
    input I2,
    input I3,
    input I4,
    input I5
)  ;
endmodule

module GTP_LUT5D
#(
    parameter [31:0] INIT = 32'h00000000
) (
    output Z,
    output Z5,
    input I0,
    input I1,
    input I2,
    input I3,
    input I4
)  ;
endmodule

module GTP_LUT4D
#(
    parameter [15:0] INIT = 16'h0000
) (
    output Z,
    output Z5,
    input I0,
    input I1,
    input I2,
    input I3
)  ;
endmodule
