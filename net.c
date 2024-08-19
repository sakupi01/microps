struct net_device {
    struct net_device *next;
    unsigned int index;
    char name[IFNAMSIZ];
    uint16_t type;
    uint16_t mtu;
    uint16_t flags;
    uint16_t hlen; /* header length */
    uint16_t alen; /* address length */
    uint8_t addr[NET_DEVICE_ADDR_LEN];
    union {
        uint8_t peer[NET_DEVICE_ADDR_LEN];
        uint8_t broadcast[NET_DEVICE_ADDR_LEN];
    };
    struct net_device_ops *ops;
    void *priv;
};

// net_deviceに実装されている関数のポインタを持つ構造体（transmit（送信関数）は重要）
struct net_device_ops {
    int (*open)(struct net_device *dev);
    int (*close)(struct net_device *dev);
    int (*transmit)(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst);
};

// ネットワークデバイスの状態を確認して、openじゃない場合はopenする
static int net_device_open(struct net_device *dev)
{
    if (NET_DEVICE_IS_UP(dev)) {
        errorf("already opened, dev=%s", dev->name);
        return -1;
    }
    if (dev->ops->open) {
        if (dev->ops->open(dev) == -1) {
            errorf("failure, dev=%s", dev->name);
            return -1;
        }
    }
    dev->flags |= NET_DEVICE_FLAG_UP;
    infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
    return 0;
}

// ネットワークデバイスの状態を確認して、closeじゃない場合はcloseする
static int net_device_close(struct net_device *dev)
{
    if (!NET_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return -1;
    }
    if (dev->ops->close) {
        if (dev->ops->close(dev) == -1) {
            errorf("failure, dev=%s", dev->name);
            return -1;
        }
    }
    dev->flags &= ~NET_DEVICE_FLAG_UP;
    infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
    return 0;
}

// ネットワークデバイスの状態を確認して、upであればデバイスからのtransmit関数を呼び出す
int net_device_output(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
    if (!NET_DEVICE_IS_UP(dev)) {
        errorf("not opened, dev=%s", dev->name);
        return -1;
    }
    if (len > dev->mtu) {
        errorf("too long, dev=%s, mtu=%u, len=%zu", dev->name, dev->mtu, len);
        return -1;
    }
    debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
    debugdump(data, len);
    if (dev->ops->transmit(dev, type, data, len, dst) == -1) {
        errorf("device transmit failure, dev=%s, len=%zu", dev->name, len);
        return -1;
    }
    return 0;
}
