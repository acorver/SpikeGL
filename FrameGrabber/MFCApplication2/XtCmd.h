#ifndef XTCMD_H
#define XTCMD_H

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>

#define XT_CMD_MAGIC 0xf33d0a76

enum XtCmds {
    XtCmd_Null = 0, XtCmd_Noop = XtCmd_Null,
    XtCmd_Img, // got image from subprocess -> SpikeGL
    XtCmd_ConsoleMessage, // basically ends up in SpikeGL's console

    XtCmd_N // num commands in enum
};

struct XtCmd {
    int magic; ///< should always equal XT_CMD_MAGIC
    int cmd; ///< one of XtCmds enum above
    int len; ///< in bytes, of the data array below!
    union {
        unsigned char data[1]; ///< the size of this array varies depending on the command in question!
        int param; ///< generic param plus also acts as padding... to make subclass members 32-bit word-aligned..
    };

    void init() { magic = XT_CMD_MAGIC; len = sizeof(int); cmd = XtCmd_Noop; param = 0; }
    XtCmd() { init();  }

    bool write(FILE *f) const { 
        size_t n = len + offsetof(XtCmd, data);
        size_t r = fwrite(this, n, 1, f);
        return r==1;
    }

    /// read data from file f and put it in output buffer "buf".  If buf is too small to fit the incoming data, buf is grown as needed to accomodate the incoming data (but is never shrunk!)
    /// returns a valid pointer into "buf" (type cast correctly) on success or NULL if there was an error.  Buf may be modified in either case!
    static XtCmd * read(std::vector<unsigned char> & buf, FILE *f) {
        int fields[3];
        size_t r, n = sizeof(int) * 3;
        r = ::fread(&fields, n, 1, f);
        XtCmd *xt = 0;
        if (r == 1 && fields[2] >= 0 && fields[0] == int(XT_CMD_MAGIC) && fields[2] <= (1024 * 1024 * 10)) { // make sure everything is kosher with the input, and that len is less than 10MB (hard limit to prevent program crashes)
            if (size_t(buf.size()) < size_t(fields[2] + n)) buf.resize(fields[2] + n);
            xt = (XtCmd *)&buf[0];
            xt->magic = fields[0]; xt->cmd = fields[1]; xt->len = fields[2]; xt->param = 0;
            if ( xt->len > 0 && ::fread(xt->data, xt->len, 1, f) != 1 ) 
                 xt = 0; // error on read, make returned pointer null
        }
        return xt;
    }

    static const XtCmd *parseBuf(const unsigned char *buf, int bufsz, int & num_consumed) {
        num_consumed = 0;
        for (int i = 0; int(i+sizeof(int)) <= bufsz; ++i) {
            int *bufi = (int *)&buf[i];
            int nrem = bufsz-i;
            if (*bufi == int(XT_CMD_MAGIC)) {
                if (nrem < (int)sizeof(XtCmd)) return 0; // don't have a full struct's full of data yet, return out of this safely
                XtCmd *xt = (XtCmd *)bufi;
                nrem -= offsetof(XtCmd,data);
                if (xt->len <= nrem) { // make sure we have all the data in the buffer, and if so, update num_consumed and return the pointer to the data
                   nrem -= xt->len;
                   num_consumed = bufsz-nrem;
                   return xt;
                }
            }
        }
        return 0; // if this is reached, num_consumed is 0 and the caller should call this function again later when more data arrives
    }

};

struct XtCmdImg : public XtCmd {
    int w, h; ///< in px, the image is always 8bit (grayscale) 
    union {
        unsigned char img[1];
        int imgPadding;
    };

    void init(int width, int height) { 
        XtCmd::init(); 
        cmd = XtCmd_Img; 
        w = width;
        h = height;
        len = (sizeof(*this) - sizeof(XtCmd)) + w*h - 1;
        imgPadding = 0;
    }
    XtCmdImg() { init(0,0);  }
};

struct XtCmdConsoleMsg : public XtCmd {
    static XtCmdConsoleMsg *allocInit(const std::string & message) {
        void *mem = malloc(sizeof(XtCmdConsoleMsg)+message.length());
        XtCmdConsoleMsg *ret = (XtCmdConsoleMsg *)mem;
        if (ret) ret->init(message);
        return ret;
    }

    void init(const std::string & message) {
        XtCmd::init();
        cmd = XtCmd_ConsoleMessage;
        len = static_cast<int>(message.length())+1;
        ::memcpy(data, &message[0], message.length());
        data[message.length()] = 0;
    }
};
#endif
