/**
 * An efficient color quantization algorithm, adapted from the C++
 * implementation quantize.c in <a
 * href="http://www.imagemagick.org/">ImageMagick</a>. The pixels for
 * an image are placed into an oct tree. The oct tree is reduced in
 * size, and the pixels from the original image are reassigned to the
 * nodes in the reduced tree.<p>
 *
 * Here is the copyright notice from ImageMagick:
 *
 * <pre>
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%  Permission is hereby granted, free of charge, to any person obtaining a    %
%  copy of this software and associated documentation files ("ImageMagick"),  %
%  to deal in ImageMagick without restriction, including without limitation   %
%  the rights to use, copy, modify, merge, publish, distribute, sublicense,   %
%  and/or sell copies of ImageMagick, and to permit persons to whom the       %
%  ImageMagick is furnished to do so, subject to the following conditions:    %
%                                                                             %
%  The above copyright notice and this permission notice shall be included in %
%  all copies or substantial portions of ImageMagick.                         %
%                                                                             %
%  The software is provided "as is", without warranty of any kind, express or %
%  implied, including but not limited to the warranties of merchantability,   %
%  fitness for a particular purpose and noninfringement.  In no event shall   %
%  E. I. du Pont de Nemours and Company be liable for any claim, damages or   %
%  other liability, whether in an action of contract, tort or otherwise,      %
%  arising from, out of or in connection with ImageMagick or the use or other %
%  dealings in ImageMagick.                                                   %
%                                                                             %
%  Except as contained in this notice, the name of the E. I. du Pont de       %
%  Nemours and Company shall not be used in advertising or otherwise to       %
%  promote the sale, use or other dealings in ImageMagick without prior       %
%  written authorization from the E. I. du Pont de Nemours and Company.       %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
</pre>
 *
 * Original author for the Java quantize.c re-implementation:
 * Written by: Adam Doppelt, version 0.90 at 19 Sep 2000
 * Source: http://www.java2s.com/Code/Java/2D-Graphics-GUI/Anefficientcolorquantizationalgorithm.htm
 *
 * Rewritten for use with c++/Qt:
 * Written by: Irmo van den Berge at 24 March 2015
 * Changes:
 * - Converted Java source code back to c++
 * - Fixed uninitialized fields and memory leaks
 * - Removed the failing 'quick' mode
 * - Replaced bit-shifting with Pixel struct/union
 * - Added QImage cross-conversion logic
 * - Added 565 color stripping option
 */
/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%           QQQ   U   U   AAA   N   N  TTTTT  IIIII   ZZZZZ  EEEEE            %
%          Q   Q  U   U  A   A  NN  N    T      I        ZZ  E                %
%          Q   Q  U   U  AAAAA  N N N    T      I      ZZZ   EEEEE            %
%          Q  QQ  U   U  A   A  N  NN    T      I     ZZ     E                %
%           QQQQ   UUU   A   A  N   N    T    IIIII   ZZZZZ  EEEEE            %
%                                                                             %
%                                                                             %
%              Reduce the Number of Unique Colors in an Image                 %
%                                                                             %
%                                                                             %
%                           Software Design                                   %
%                             John Cristy                                     %
%                              July 1992                                      %
%                                                                             %
%                                                                             %
%  Copyright 1998 E. I. du Pont de Nemours and Company                        %
%                                                                             %
%  Permission is hereby granted, free of charge, to any person obtaining a    %
%  copy of this software and associated documentation files ("ImageMagick"),  %
%  to deal in ImageMagick without restriction, including without limitation   %
%  the rights to use, copy, modify, merge, publish, distribute, sublicense,   %
%  and/or sell copies of ImageMagick, and to permit persons to whom the       %
%  ImageMagick is furnished to do so, subject to the following conditions:    %
%                                                                             %
%  The above copyright notice and this permission notice shall be included in %
%  all copies or substantial portions of ImageMagick.                         %
%                                                                             %
%  The software is provided "as is", without warranty of any kind, express or %
%  implied, including but not limited to the warranties of merchantability,   %
%  fitness for a particular purpose and noninfringement.  In no event shall   %
%  E. I. du Pont de Nemours and Company be liable for any claim, damages or   %
%  other liability, whether in an action of contract, tort or otherwise,      %
%  arising from, out of or in connection with ImageMagick or the use or other %
%  dealings in ImageMagick.                                                   %
%                                                                             %
%  Except as contained in this notice, the name of the E. I. du Pont de       %
%  Nemours and Company shall not be used in advertising or otherwise to       %
%  promote the sale, use or other dealings in ImageMagick without prior       %
%  written authorization from the E. I. du Pont de Nemours and Company.       %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  Realism in computer graphics typically requires using 24 bits/pixel to
%  generate an image. Yet many graphic display devices do not contain
%  the amount of memory necessary to match the spatial and color
%  resolution of the human eye. The QUANTIZE program takes a 24 bit
%  image and reduces the number of colors so it can be displayed on
%  raster device with less bits per pixel. In most instances, the
%  quantized image closely resembles the original reference image.
%
%  A reduction of colors in an image is also desirable for image
%  transmission and real-time animation.
%
%  Function Quantize takes a standard RGB or monochrome images and quantizes
%  them down to some fixed number of colors.
%
%  For purposes of color allocation, an image is a set of n pixels, where
%  each pixel is a point in RGB space. RGB space is a 3-dimensional
%  vector space, and each pixel, pi, is defined by an ordered triple of
%  red, green, and blue coordinates, (ri, gi, bi).
%
%  Each primary color component (red, green, or blue) represents an
%  intensity which varies linearly from 0 to a maximum value, cmax, which
%  corresponds to full saturation of that color. Color allocation is
%  defined over a domain consisting of the cube in RGB space with
%  opposite vertices at (0,0,0) and (cmax,cmax,cmax). QUANTIZE requires
%  cmax = 255.
%
%  The algorithm maps this domain onto a tree in which each node
%  represents a cube within that domain. In the following discussion
%  these cubes are defined by the coordinate of two opposite vertices:
%  The vertex nearest the origin in RGB space and the vertex farthest
%  from the origin.
%
%  The tree's root node represents the the entire domain, (0,0,0) through
%  (cmax,cmax,cmax). Each lower level in the tree is generated by
%  subdividing one node's cube into eight smaller cubes of equal size.
%  This corresponds to bisecting the parent cube with planes passing
%  through the midpoints of each edge.
%
%  The basic algorithm operates in three phases: Classification,
%  Reduction, and Assignment. Classification builds a color
%  description tree for the image. Reduction collapses the tree until
%  the number it represents, at most, the number of colors desired in the
%  output image. Assignment defines the output image's color map and
%  sets each pixel's color by reclassification in the reduced tree.
%  Our goal is to minimize the numerical discrepancies between the original
%  colors and quantized colors (quantization error).
%
%  Classification begins by initializing a color description tree of
%  sufficient depth to represent each possible input color in a leaf.
%  However, it is impractical to generate a fully-formed color
%  description tree in the classification phase for realistic values of
%  cmax. If colors components in the input image are quantized to k-bit
%  precision, so that cmax= 2k-1, the tree would need k levels below the
%  root node to allow representing each possible input color in a leaf.
%  This becomes prohibitive because the tree's total number of nodes is
%  1 + sum(i=1,k,8k).
%
%  A complete tree would require 19,173,961 nodes for k = 8, cmax = 255.
%  Therefore, to avoid building a fully populated tree, QUANTIZE: (1)
%  Initializes data structures for nodes only as they are needed;  (2)
%  Chooses a maximum depth for the tree as a function of the desired
%  number of colors in the output image (currently log2(colormap size)).
%
%  For each pixel in the input image, classification scans downward from
%  the root of the color description tree. At each level of the tree it
%  identifies the single node which represents a cube in RGB space
%  containing the pixel's color. It updates the following data for each
%  such node:
%
%    n1: Number of pixels whose color is contained in the RGB cube
%    which this node represents;
%
%    n2: Number of pixels whose color is not represented in a node at
%    lower depth in the tree;  initially,  n2 = 0 for all nodes except
%    leaves of the tree.
%
%    Sr, Sg, Sb: Sums of the red, green, and blue component values for
%    all pixels not classified at a lower depth. The combination of
%    these sums and n2  will ultimately characterize the mean color of a
%    set of pixels represented by this node.
%
%    E: The distance squared in RGB space between each pixel contained
%    within a node and the nodes' center. This represents the quantization
%    error for a node.
%
%  Reduction repeatedly prunes the tree until the number of nodes with
%  n2 > 0 is less than or equal to the maximum number of colors allowed
%  in the output image. On any given iteration over the tree, it selects
%  those nodes whose E count is minimal for pruning and merges their
%  color statistics upward. It uses a pruning threshold, Ep, to govern
%  node selection as follows:
%
%    Ep = 0
%    while number of nodes with (n2 > 0) > required maximum number of colors
%      prune all nodes such that E <= Ep
%      Set Ep to minimum E in remaining nodes
%
%  This has the effect of minimizing any quantization error when merging
%  two nodes together.
%
%  When a node to be pruned has offspring, the pruning procedure invokes
%  itself recursively in order to prune the tree from the leaves upward.
%  n2,  Sr, Sg,  and  Sb in a node being pruned are always added to the
%  corresponding data in that node's parent. This retains the pruned
%  node's color characteristics for later averaging.
%
%  For each node, n2 pixels exist for which that node represents the
%  smallest volume in RGB space containing those pixel's colors. When n2
%  > 0 the node will uniquely define a color in the output image. At the
%  beginning of reduction,  n2 = 0  for all nodes except a the leaves of
%  the tree which represent colors present in the input image.
%
%  The other pixel count, n1, indicates the total number of colors
%  within the cubic volume which the node represents. This includes n1 -
%  n2  pixels whose colors should be defined by nodes at a lower level in
%  the tree.
%
%  Assignment generates the output image from the pruned tree. The
%  output image consists of two parts: (1)  A color map, which is an
%  array of color descriptions (RGB triples) for each color present in
%  the output image;  (2)  A pixel array, which represents each pixel as
%  an index into the color map array.
%
%  First, the assignment phase makes one pass over the pruned color
%  description tree to establish the image's color map. For each node
%  with n2  > 0, it divides Sr, Sg, and Sb by n2 . This produces the
%  mean color of all pixels that classify no lower than this node. Each
%  of these colors becomes an entry in the color map.
%
%  Finally,  the assignment phase reclassifies each pixel in the pruned
%  tree to identify the deepest node containing the pixel's color. The
%  pixel's value in the pixel array becomes the index of this node's mean
%  color in the color map.
%
%  With the permission of USC Information Sciences Institute, 4676 Admiralty
%  Way, Marina del Rey, California  90292, this code was adapted from module
%  ALCOLS written by Paul Raveling.
%
%  The names of ISI and USC are not used in advertising or publicity
%  pertaining to distribution of the software without prior specific
%  written permission from ISI.
%
*/
#ifndef QUANTIZE_H
#define QUANTIZE_H

#include <QImage>
#include <QColor>
#include <QDebug>

namespace Quantize {

const int MAX_RGB = 0xFF;
const int MAX_NODES = 266817;
const int MAX_TREE_DEPTH = 8;

/**
 * RGB Pixel
 */
typedef struct Pixel_ARGB {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
} Pixel_RGB;

/**
 * A single quantizable pixel
 */
typedef union Pixel {
    Pixel_ARGB color;
    uint value;
    QRgb rgb;
} Pixel;

/**
 * The result of a closest color search.
 */
typedef struct Search {
    int distance;
    int color_number;
} Search;

/**
 * Figure out the euclidian distance between two pixel colors
 */
int calc_distance(Pixel &color1, Pixel &color2);

// Predefined
class Node;

class Cube {
public:
    Pixel **pixels;
    int max_colors;
    Pixel *colormap;

    // whether data is stored as true color as opposed to color mapped
    bool trueColor;

    // depth the quantization process went
    int depth;
    // root node of the octree
    Node *root;
    // counter for the number of nodes in the tree
    int nodes;

    // counter for the number of colors in the cube. this gets
    // recalculated often.
    int colors;

    // Dimensions of the image
    int width;
    int height;

    /*
     * Initializer specifying the pixel ARGB data and max colors to quantize to
     */
    Cube();

    /*
     * Deconstructor for deleting all quantization data and temporary nodes
     */
    ~Cube();

    /*
     * Erases all contents from previous quantization
     */
    void erase();

    /*
     * Loads image data without applying to the output image
     */
    void loadImage(QImage &image);

    /*
     * Performs quantization on the currently loaded data
     */
    void reduce(int max_colors);

    /*
     * Sets whether to represent the data in True Color format
     * or to use a colormap instead.
     */
    void setTrueColor(bool trueColor);

    /*
     * Initializes or resizes the colormap to a given size
     */
    void setColormapSize(int colorCount);

    /*
     * Swaps the place of two colors in the colormap
     */
    void swapColors(uint indexA, uint indexB);

    /*
     * Converts the contents stored into a usable QImage
     */
    QImage toImage();

    /*
     * Procedure Classification begins by initializing a color
     * description tree of sufficient depth to represent each
     * possible input color in a leaf. However, it is impractical
     * to generate a fully-formed color description tree in the
     * classification phase for realistic values of cmax. If
     * colors components in the input image are quantized to k-bit
     * precision, so that cmax= 2k-1, the tree would need k levels
     * below the root node to allow representing each possible
     * input color in a leaf. This becomes prohibitive because the
     * tree's total number of nodes is 1 + sum(i=1,k,8k).
     *
     * A complete tree would require 19,173,961 nodes for k = 8,
     * cmax = 255. Therefore, to avoid building a fully populated
     * tree, QUANTIZE: (1) Initializes data structures for nodes
     * only as they are needed; (2) Chooses a maximum depth for
     * the tree as a function of the desired number of colors in
     * the output image (currently log2(colormap size)).
     *
     * For each pixel in the input image, classification scans
     * downward from the root of the color description tree. At
     * each level of the tree it identifies the single node which
     * represents a cube in RGB space containing It updates the
     * following data for each such node:
     *
     *   number_pixels : Number of pixels whose color is contained
     *   in the RGB cube which this node represents;
     *
     *   unique : Number of pixels whose color is not represented
     *   in a node at lower depth in the tree; initially, n2 = 0
     *   for all nodes except leaves of the tree.
     *
     *   total_red/green/blue : Sums of the red, green, and blue
     *   component values for all pixels not classified at a lower
     *   depth. The combination of these sums and n2 will
     *   ultimately characterize the mean color of a set of pixels
     *   represented by this node.
     */
    void classification();
    /*
     * reduction repeatedly prunes the tree until the number of
     * nodes with unique > 0 is less than or equal to the maximum
     * number of colors allowed in the output image.
     *
     * When a node to be pruned has offspring, the pruning
     * procedure invokes itself recursively in order to prune the
     * tree from the leaves upward.  The statistics of the node
     * being pruned are always added to the corresponding data in
     * that node's parent.  This retains the pruned node's color
     * characteristics for later averaging.
     */
    void reduction();
    /*
     * Procedure assignment generates the output image from the
     * pruned tree. The output image consists of two parts: (1) A
     * color map, which is an array of color descriptions (RGB
     * triples) for each color present in the output image; (2) A
     * pixel array, which represents each pixel as an index into
     * the color map array.
     *
     * First, the assignment phase makes one pass over the pruned
     * color description tree to establish the image's color map.
     * For each node with n2 > 0, it divides Sr, Sg, and Sb by n2.
     * This produces the mean color of all pixels that classify no
     * lower than this node. Each of these colors becomes an entry
     * in the color map.
     *
     * Finally, the assignment phase reclassifies each pixel in
     * the pruned tree to identify the deepest node containing the
     * pixel's color. The pixel's value in the pixel array becomes
     * the index of this node's mean color in the color map.
     */
    void assignment();
    /*
     * Sorts the colors in the colormap on hue, from dark to light.
     */
    void sortColors();

    /*
     * Finds the index in the colormap for the color closest
     */
    uint findColor(Pixel color, uint startIndex = 0);
};

/**
 * A single Node in the tree.
 */
class Node {
public:
    Cube *cube;

    // parent node
    Node *parent;

    // child nodes
    Node **child;
    int nchild;

    // our index within our parent
    int id;
    // our level within the tree
    int level;
    // our color midpoint
    int mid_red;
    int mid_green;
    int mid_blue;

    // the pixel count for this node and all children
    int number_pixels;

    // the pixel count for this node
    int unique;
    // the sum of all pixels contained in this node
    int total_red;
    int total_green;
    int total_blue;

    // used to build the colormap
    int color_number;

    Node(Cube *cube);
    Node(Node *parent, int id, int level);
    ~Node();

    /**
     * Initializes the Node to the default values
     */
    void init();

    /**
     * Remove this child node, and make sure our parent
     * absorbs our pixel statistics.
     */
    void pruneChild();

    /**
     * Prune the lowest layer of the tree.
     */
    void pruneLevel();

    /**
     * Remove any nodes that have fewer than threshold
     * pixels. Also, as long as we're walking the tree:
     *
     *  - figure out the color with the fewest pixels
     *  - recalculate the total number of colors in the tree
     */
    int reduce(int threshold, int next_threshold);

    /*
     * colormap traverses the color cube tree and notes each
     * colormap entry. A colormap entry is any node in the
     * color cube tree where the number of unique colors is
     * not zero.
     */
    void colormap();

    /* ClosestColor traverses the color cube tree at a
     * particular node and determines which colormap entry
     * best represents the input color.
     */
    void closestColor(Pixel &pixel, Search *search);


};

}

#endif // QUANTIZE_H
