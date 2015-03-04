#ifndef XTCMD_H
#define XTCMD_H

#include <cstdio>
#include <cstdlib>
#include <vector>

#define XT_CMD_MAGIC 0xf33d0a76

enum XtCmds {
    XtCmd_Null = 0, XtCmd_Noop = XtCmd_Null,
    XtCmd_Img, // got image from subprocess -> SpikeGL

    XtCmd_N // num commands in enum
};

struct XtCmd {
    int magic; ///< should always equal XT_CMD_MAGIC
    int cmd; ///< one of XtCmds enum above
    int len; ///< in bytes, of the data array below!
    unsigned char data[1]; ///< the size of this array varies depending on the command in question!

    void init() { magic = XT_CMD_MAGIC; len = 1; cmd = XtCmd_Noop; }
    XtCmd() { init();  }

    bool write(FILE *f) const { 
        size_t n = len + (data - (unsigned char *)(this));
        size_t r = fwrite(this, n, 1, f);
        return r==1;
    }

    /// read data from file f and put it in output buffer "buf".  If buf is too small to fit the incoming data, buf is grown as needed to accomodate the incoming data (but is never shrunk!)
    /// returns a valid pointer into "buf" (type cast correctly) on success or NULL if there was an error.  Buf may be modified in either case!
    static XtCmd * read(std::vector<unsigned char> & buf, FILE *f) {
        int fields[3];
        size_t r, n = sizeof(int) * 3;
        r = ::fread(&fields, 1, n, f);
        XtCmd *xt = 0;
        if (r == n && fields[2] >= 0 && fields[0] == XT_CMD_MAGIC && fields[2] <= (1024 * 1024 * 10)) { // make sure everything is kosher with the input, and that len is less than 10MB (hard limit to prevent program crashes)
            if (size_t(buf.size()) < size_t(fields[2] + n)) buf.resize(fields[2] + n);
            xt = (XtCmd *)&buf[0];
            xt->magic = fields[0]; xt->cmd = fields[1]; xt->len = fields[2];
            if ( xt->len > 0 && ::fread(xt->data, xt->len, 1, f) != 1 ) 
                 xt = 0; // error on read, make returned pointer null
        }
        return xt;
    }

};

struct XtCmdImg : public XtCmd {
    int w, h; ///< in px, the image is always 8bit (grayscale) 
    unsigned char img[1];
    void init() { XtCmd::init(); cmd = XtCmd_Img; w = h = 0;  }
    XtCmdImg() { init();  }
};


#endif
