// Benchmark "tests/tc_public/tc_public_1/output" written by ABC on Wed Feb  4 21:53:13 2026

module \tests/tc_public/tc_public_1/output  ( 
    pi0, pi1, pi2, pi3, pi4, pi5, pi6, pi7, pi8, pi9,
    po0, po1, po2, po3, po4, po5, po6, po7  );
  input  pi0, pi1, pi2, pi3, pi4, pi5, pi6, pi7, pi8, pi9;
  output po0, po1, po2, po3, po4, po5, po6, po7;
  wire new_n19, new_n20, new_n21, new_n22, new_n23, new_n24, new_n25,
    new_n26, new_n27, new_n28, new_n29, new_n30, new_n31, new_n32, new_n33,
    new_n34, new_n35, new_n36, new_n37, new_n38, new_n39, new_n40, new_n42,
    new_n43, new_n44, new_n45, new_n46, new_n47, new_n48, new_n49, new_n50,
    new_n51, new_n52, new_n54, new_n55, new_n56, new_n57, new_n58, new_n59,
    new_n60, new_n61, new_n62, new_n63, new_n64, new_n65, new_n66, new_n67,
    new_n68, new_n69, new_n70, new_n71, new_n72, new_n73, new_n74, new_n75,
    new_n76, new_n77, new_n78, new_n79, new_n81, new_n82, new_n83, new_n84,
    new_n85, new_n86, new_n87, new_n88, new_n89, new_n90, new_n91, new_n92,
    new_n93, new_n94, new_n95, new_n96, new_n98, new_n99, new_n100,
    new_n101, new_n102, new_n103, new_n104, new_n105, new_n106, new_n107,
    new_n108, new_n109, new_n110, new_n111, new_n112, new_n113, new_n115,
    new_n116, new_n117, new_n118, new_n119, new_n120, new_n121, new_n122,
    new_n123, new_n124, new_n125, new_n126, new_n127, new_n128, new_n129,
    new_n130, new_n131, new_n132, new_n133, new_n134, new_n135, new_n136,
    new_n137, new_n138, new_n139, new_n140, new_n141, new_n142, new_n143,
    new_n144, new_n145, new_n146, new_n147, new_n149, new_n150, new_n151,
    new_n152, new_n153, new_n154, new_n155, new_n156, new_n157, new_n158,
    new_n159, new_n160, new_n161, new_n162, new_n163, new_n164, new_n165,
    new_n166, new_n167, new_n168, new_n169, new_n170, new_n171, new_n172,
    new_n173, new_n174, new_n175, new_n176, new_n177, new_n178, new_n179,
    new_n180, new_n181, new_n182, new_n183, new_n184, new_n185, new_n186,
    new_n188, new_n189, new_n190, new_n191, new_n192, new_n193, new_n194,
    new_n195, new_n196, new_n197, new_n198, new_n199, new_n200, new_n201,
    new_n202, new_n203, new_n204, new_n205, new_n206, new_n207, new_n208,
    new_n209, new_n210, new_n211, new_n212, new_n213, new_n214, new_n215,
    new_n216, new_n217, new_n218, new_n219, new_n220, new_n221, new_n222,
    new_n223, new_n224;
  assign new_n19 = pi6 & pi7;
  assign new_n20 = ~pi6 & ~pi7;
  assign new_n21 = ~new_n19 & ~new_n20;
  assign new_n22 = pi1 & ~pi2;
  assign new_n23 = pi4 & pi5;
  assign new_n24 = ~pi3 & new_n23;
  assign new_n25 = new_n22 & new_n24;
  assign new_n26 = ~pi1 & pi2;
  assign new_n27 = ~pi4 & ~pi5;
  assign new_n28 = pi3 & new_n27;
  assign new_n29 = new_n26 & new_n28;
  assign new_n30 = ~new_n25 & ~new_n29;
  assign new_n31 = pi0 & ~new_n30;
  assign new_n32 = ~pi1 & ~pi2;
  assign new_n33 = new_n24 & new_n32;
  assign new_n34 = pi1 & pi2;
  assign new_n35 = new_n28 & new_n34;
  assign new_n36 = ~new_n33 & ~new_n35;
  assign new_n37 = ~pi0 & ~new_n36;
  assign new_n38 = ~new_n31 & ~new_n37;
  assign new_n39 = ~new_n21 & ~new_n38;
  assign new_n40 = ~pi9 & new_n39;
  assign po0 = ~pi8 & new_n40;
  assign new_n42 = ~pi8 & pi9;
  assign new_n43 = pi8 & ~pi9;
  assign new_n44 = ~new_n42 & ~new_n43;
  assign new_n45 = ~pi0 & pi1;
  assign new_n46 = pi0 & ~pi1;
  assign new_n47 = ~new_n45 & ~new_n46;
  assign new_n48 = ~new_n44 & ~new_n47;
  assign new_n49 = pi7 & new_n48;
  assign new_n50 = ~pi6 & new_n49;
  assign new_n51 = pi5 & new_n50;
  assign new_n52 = ~pi4 & new_n51;
  assign po1 = pi3 & new_n52;
  assign new_n54 = ~pi3 & pi5;
  assign new_n55 = pi1 & new_n54;
  assign new_n56 = pi3 & ~pi5;
  assign new_n57 = ~pi1 & new_n56;
  assign new_n58 = ~new_n55 & ~new_n57;
  assign new_n59 = pi0 & ~new_n58;
  assign new_n60 = ~pi1 & new_n54;
  assign new_n61 = pi1 & new_n56;
  assign new_n62 = ~new_n60 & ~new_n61;
  assign new_n63 = ~pi0 & ~new_n62;
  assign new_n64 = ~new_n59 & ~new_n63;
  assign new_n65 = ~pi2 & ~new_n64;
  assign new_n66 = ~pi5 & ~new_n47;
  assign new_n67 = pi2 & new_n66;
  assign new_n68 = ~new_n65 & ~new_n67;
  assign new_n69 = ~pi4 & ~new_n68;
  assign new_n70 = pi0 & pi1;
  assign new_n71 = ~pi0 & ~pi1;
  assign new_n72 = ~new_n70 & ~new_n71;
  assign new_n73 = pi4 & ~new_n72;
  assign new_n74 = ~pi3 & new_n73;
  assign new_n75 = ~pi2 & new_n74;
  assign new_n76 = ~new_n69 & ~new_n75;
  assign new_n77 = ~pi9 & ~new_n76;
  assign new_n78 = ~pi8 & new_n77;
  assign new_n79 = ~pi7 & new_n78;
  assign po2 = ~pi6 & new_n79;
  assign new_n81 = ~pi2 & pi3;
  assign new_n82 = pi2 & ~pi3;
  assign new_n83 = ~new_n81 & ~new_n82;
  assign new_n84 = ~new_n72 & ~new_n83;
  assign new_n85 = pi5 & new_n84;
  assign new_n86 = pi3 & new_n66;
  assign new_n87 = pi2 & new_n86;
  assign new_n88 = ~new_n85 & ~new_n87;
  assign new_n89 = ~pi4 & ~new_n88;
  assign new_n90 = pi4 & new_n66;
  assign new_n91 = ~pi3 & new_n90;
  assign new_n92 = pi2 & new_n91;
  assign new_n93 = ~new_n89 & ~new_n92;
  assign new_n94 = ~pi9 & ~new_n93;
  assign new_n95 = ~pi8 & new_n94;
  assign new_n96 = ~pi7 & new_n95;
  assign po3 = ~pi6 & new_n96;
  assign new_n98 = ~pi2 & pi5;
  assign new_n99 = pi2 & ~pi5;
  assign new_n100 = ~new_n98 & ~new_n99;
  assign new_n101 = ~new_n72 & ~new_n100;
  assign new_n102 = ~pi3 & new_n101;
  assign new_n103 = ~new_n87 & ~new_n102;
  assign new_n104 = ~pi4 & ~new_n103;
  assign new_n105 = pi0 & ~new_n62;
  assign new_n106 = ~pi0 & ~new_n58;
  assign new_n107 = ~new_n105 & ~new_n106;
  assign new_n108 = pi4 & ~new_n107;
  assign new_n109 = ~pi2 & new_n108;
  assign new_n110 = ~new_n104 & ~new_n109;
  assign new_n111 = ~pi9 & ~new_n110;
  assign new_n112 = ~pi8 & new_n111;
  assign new_n113 = ~pi7 & new_n112;
  assign po4 = ~pi6 & new_n113;
  assign new_n115 = ~pi2 & ~pi3;
  assign new_n116 = ~pi5 & pi6;
  assign new_n117 = new_n115 & new_n116;
  assign new_n118 = pi2 & pi3;
  assign new_n119 = pi5 & ~pi6;
  assign new_n120 = new_n118 & new_n119;
  assign new_n121 = ~new_n117 & ~new_n120;
  assign new_n122 = ~new_n47 & ~new_n121;
  assign new_n123 = pi7 & new_n122;
  assign new_n124 = ~pi7 & ~new_n72;
  assign new_n125 = pi6 & new_n124;
  assign new_n126 = pi5 & new_n125;
  assign new_n127 = pi3 & new_n126;
  assign new_n128 = ~pi2 & new_n127;
  assign new_n129 = ~new_n123 & ~new_n128;
  assign new_n130 = ~pi4 & ~new_n129;
  assign new_n131 = ~pi6 & pi7;
  assign new_n132 = pi1 & new_n131;
  assign new_n133 = pi6 & ~pi7;
  assign new_n134 = ~pi1 & new_n133;
  assign new_n135 = ~new_n132 & ~new_n134;
  assign new_n136 = pi0 & ~new_n135;
  assign new_n137 = ~pi1 & new_n131;
  assign new_n138 = pi1 & new_n133;
  assign new_n139 = ~new_n137 & ~new_n138;
  assign new_n140 = ~pi0 & ~new_n139;
  assign new_n141 = ~new_n136 & ~new_n140;
  assign new_n142 = ~pi5 & ~new_n141;
  assign new_n143 = pi4 & new_n142;
  assign new_n144 = ~pi3 & new_n143;
  assign new_n145 = pi2 & new_n144;
  assign new_n146 = ~new_n130 & ~new_n145;
  assign new_n147 = ~pi9 & ~new_n146;
  assign po5 = ~pi8 & new_n147;
  assign new_n149 = pi6 & pi8;
  assign new_n150 = ~pi5 & new_n149;
  assign new_n151 = new_n115 & new_n150;
  assign new_n152 = ~pi6 & ~pi8;
  assign new_n153 = pi5 & new_n152;
  assign new_n154 = new_n118 & new_n153;
  assign new_n155 = ~new_n151 & ~new_n154;
  assign new_n156 = ~new_n47 & ~new_n155;
  assign new_n157 = pi9 & new_n156;
  assign new_n158 = pi7 & new_n157;
  assign new_n159 = ~pi9 & ~new_n72;
  assign new_n160 = pi8 & new_n159;
  assign new_n161 = ~pi7 & new_n160;
  assign new_n162 = pi6 & new_n161;
  assign new_n163 = pi5 & new_n162;
  assign new_n164 = pi3 & new_n163;
  assign new_n165 = ~pi2 & new_n164;
  assign new_n166 = ~new_n158 & ~new_n165;
  assign new_n167 = ~pi4 & ~new_n166;
  assign new_n168 = pi1 & ~pi6;
  assign new_n169 = pi7 & new_n42;
  assign new_n170 = new_n168 & new_n169;
  assign new_n171 = ~pi1 & pi6;
  assign new_n172 = ~pi7 & new_n43;
  assign new_n173 = new_n171 & new_n172;
  assign new_n174 = ~new_n170 & ~new_n173;
  assign new_n175 = pi0 & ~new_n174;
  assign new_n176 = ~pi1 & ~pi6;
  assign new_n177 = new_n169 & new_n176;
  assign new_n178 = pi1 & pi6;
  assign new_n179 = new_n172 & new_n178;
  assign new_n180 = ~new_n177 & ~new_n179;
  assign new_n181 = ~pi0 & ~new_n180;
  assign new_n182 = ~new_n175 & ~new_n181;
  assign new_n183 = ~pi5 & ~new_n182;
  assign new_n184 = pi4 & new_n183;
  assign new_n185 = ~pi3 & new_n184;
  assign new_n186 = pi2 & new_n185;
  assign po6 = new_n167 | new_n186;
  assign new_n188 = pi4 & ~pi6;
  assign new_n189 = pi1 & new_n188;
  assign new_n190 = pi8 & pi9;
  assign new_n191 = ~pi7 & new_n190;
  assign new_n192 = new_n189 & new_n191;
  assign new_n193 = ~pi4 & pi6;
  assign new_n194 = ~pi1 & new_n193;
  assign new_n195 = ~pi8 & ~pi9;
  assign new_n196 = pi7 & new_n195;
  assign new_n197 = new_n194 & new_n196;
  assign new_n198 = ~new_n192 & ~new_n197;
  assign new_n199 = pi0 & ~pi2;
  assign new_n200 = new_n54 & new_n199;
  assign new_n201 = ~pi0 & pi2;
  assign new_n202 = new_n56 & new_n201;
  assign new_n203 = ~new_n200 & ~new_n202;
  assign new_n204 = ~new_n198 & ~new_n203;
  assign new_n205 = ~pi1 & new_n188;
  assign new_n206 = new_n191 & new_n205;
  assign new_n207 = pi1 & new_n193;
  assign new_n208 = new_n196 & new_n207;
  assign new_n209 = ~new_n206 & ~new_n208;
  assign new_n210 = ~pi0 & ~pi2;
  assign new_n211 = new_n54 & new_n210;
  assign new_n212 = pi0 & pi2;
  assign new_n213 = new_n56 & new_n212;
  assign new_n214 = ~new_n211 & ~new_n213;
  assign new_n215 = ~new_n209 & ~new_n214;
  assign new_n216 = pi9 & ~new_n47;
  assign new_n217 = ~pi8 & new_n216;
  assign new_n218 = pi7 & new_n217;
  assign new_n219 = ~pi6 & new_n218;
  assign new_n220 = pi5 & new_n219;
  assign new_n221 = ~pi4 & new_n220;
  assign new_n222 = ~pi3 & new_n221;
  assign new_n223 = pi2 & new_n222;
  assign new_n224 = ~new_n215 & ~new_n223;
  assign po7 = new_n204 | ~new_n224;
endmodule


