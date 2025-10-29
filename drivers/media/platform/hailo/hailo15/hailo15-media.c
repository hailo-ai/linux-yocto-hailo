#include "common.h"
#include "hailo15-media.h"

struct list_head connections;
struct list_head endpoints_list;
struct media_device mdev;
struct mutex mdev_lock;
int init;

struct media_device* hailo15_media_get_media_device(){
	return &mdev;
}
EXPORT_SYMBOL(hailo15_media_get_media_device);

static int hailo15_media_register_subdevice(struct hailo15_media_device* mdev){
	mutex_lock(&mdev_lock);
	list_add_tail(&mdev->link, &endpoints_list);
	mutex_unlock(&mdev_lock);
	return 0;
}

static struct hailo15_media_device* hailo15_media_get_endpoint(struct fwnode_handle* remote){
	struct list_head *pos, *npos;
	struct hailo15_media_device* dev;
	mutex_lock(&mdev_lock);
	list_for_each_safe(pos, npos, &endpoints_list){
		dev = list_entry(pos, struct hailo15_media_device, link);
		if(!dev){
			mutex_unlock(&mdev_lock);
			return NULL;
		}
		if(dev->endpoint == remote){
			mutex_unlock(&mdev_lock);
			return dev;
		}
	}
	mutex_unlock(&mdev_lock);
	return NULL;
}

static int hailo15_media_remote_handle_available(struct fwnode_handle* remote){
	return hailo15_media_get_endpoint(remote) != NULL;
}

static int hailo15_media_register_subdev_to_v4l2(struct hailo15_media_device* mdev, struct v4l2_device* v4l2_dev){
	struct hailo15_media_connection* pos, *npos;
	int ret;
	list_for_each_entry_safe(pos, npos, &connections, connection){
		if(pos->sink->sd != mdev->sd)
			continue;
		if(pos->source->sd->v4l2_dev == NULL)
			hailo15_media_register_subdev_to_v4l2(pos->source, v4l2_dev);
	}

	if(mdev->sd->v4l2_dev == NULL){
		dev_dbg(mdev->sd->dev, "Registering subdev %s to v4l2 device %s\n", mdev->sd->name, v4l2_dev->name);
		mutex_unlock(&mdev_lock);
		ret = v4l2_device_register_subdev(v4l2_dev, mdev->sd);
		mutex_lock(&mdev_lock);
		return ret;

	}
	return 0;
}

int hailo15_media_device_initialized(){
	return init;
}
EXPORT_SYMBOL(hailo15_media_device_initialized);


void hailo15_media_init_media_device(struct device* dev){
	int ret;
	mdev.dev = dev;
	media_device_init(&mdev);
	ret = media_device_register(&mdev);
	if(ret)
		pr_err("media device registration failed!\n");
	init = 1;
}
EXPORT_SYMBOL(hailo15_media_init_media_device);

int hailo15_media_register_v4l2_device(struct v4l2_device* v4l2_dev, int id){
	struct fwnode_handle *handle, *remote_handle;
	struct device* dev = v4l2_dev->dev;
	struct hailo15_media_device* mdev;
	int sink = 0, ret = -EINVAL;
	int reg = 0;

	dev_dbg(dev, "Registering v4l2 device %s\n", v4l2_dev->name);

	fwnode_graph_for_each_endpoint(dev_fwnode(dev), handle){
		struct fwnode_handle *remote_parent;
		ret = fwnode_property_read_u32(handle, "sink", &sink);
		if(ret || !sink){
			continue;
		}

		ret = fwnode_property_read_u32(handle, "reg", &reg);
		if(ret)
			continue;
		if(reg != id)
			continue;

		remote_handle = fwnode_graph_get_remote_endpoint(handle);
		if(!remote_handle){
			fwnode_handle_put(handle);
			return -EINVAL;
		}

		/* Handle cases were parent is not available (status != ok in dt) */
		remote_parent = fwnode_graph_get_port_parent(remote_handle);
		if (remote_parent && !fwnode_device_is_available(remote_parent)) {
			pr_info("skipping remote-ep %s because its device is not available\n",
				 remote_handle->ops->get_name(remote_handle));
			continue;
		}

		if(!hailo15_media_remote_handle_available(remote_handle)){
			fwnode_handle_put(handle);
			fwnode_handle_put(remote_handle);
			return -EPROBE_DEFER;
		}
		mdev = hailo15_media_get_endpoint(remote_handle);
		mutex_lock(&mdev_lock);
		ret = hailo15_media_register_subdev_to_v4l2(mdev, v4l2_dev);
		mutex_unlock(&mdev_lock);
		fwnode_handle_put(handle);
		fwnode_handle_put(remote_handle);
		break;
	}

	return ret;
}
EXPORT_SYMBOL(hailo15_media_register_v4l2_device);

int hailo15_media_get_sink_endpoints_status(struct device* dev){
	struct fwnode_handle *handle, *remote_handle;
	struct fwnode_endpoint fwnode_ep, fwnode_remote_ep;
	int sink = 0, ret = 0;

	fwnode_graph_for_each_endpoint(dev_fwnode(dev), handle){
		struct fwnode_handle *remote_parent;
		ret = fwnode_property_read_u32(handle, "sink", &sink);
		if(ret || !sink){
			continue;
		}

		ret = fwnode_graph_parse_endpoint(handle, &fwnode_ep);
		if (ret) {
			pr_err("failed to parse sink-ep of node %s, skipping...", dev_name(dev));
			continue;
		}

		remote_handle = fwnode_graph_get_remote_endpoint(handle);
		if(!remote_handle){
			pr_err("failed to get remote-ep connected to sink-ep: [%s, port %d, id %d], aborting...",
				 dev_name(dev), fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(handle);
			return -EINVAL;
		}

		/* Handle cases were parent is not available (status != ok in dt) */
		remote_parent = fwnode_graph_get_port_parent(remote_handle);
		if (!remote_parent) {
			pr_err("failed to get parent of remote-ep connected to sink-ep: [%s, port %d, id %d], aborting...",
				 dev_name(dev), fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(handle);
			fwnode_handle_put(remote_handle);
			return -EINVAL;
		}

		if (!fwnode_device_is_available(remote_parent)) {
			pr_debug("parent node of remote-ep: [%s, port %d, id %d] connected to sink-ep: [%s, port %d, id %d], is not available, skipping...\n",
				 remote_parent->ops->get_name(remote_parent), fwnode_remote_ep.port, fwnode_remote_ep.id, 
				 dev_name(dev), fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(remote_handle);
			fwnode_handle_put(remote_parent);
			continue;
		}

		if(!hailo15_media_remote_handle_available(remote_handle)){
			pr_debug("remote-ep: [%s, port %d, id %d] connected to sink-ep: [%s, port %d, id %d], parent remote-ep not registered, aborting with -EPROBE_DEFER\n",
				 remote_parent->ops->get_name(remote_parent), fwnode_remote_ep.port, fwnode_remote_ep.id, 
				 dev_name(dev), fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(handle);
			fwnode_handle_put(remote_handle);
			fwnode_handle_put(remote_parent);
			return -EPROBE_DEFER;
		}

		fwnode_handle_put(remote_handle);
		fwnode_handle_put(remote_parent);
	}

	return 0;
}
EXPORT_SYMBOL(hailo15_media_get_sink_endpoints_status);

int hailo15_media_get_subdev(struct device *dev, int id, struct v4l2_subdev **sd){
	struct fwnode_handle *handle, *remote_handle;
	struct hailo15_media_device *med_dev;
	int ret = 0;
	int sink = 0;
	int reg = 0;

	fwnode_graph_for_each_endpoint(dev_fwnode(dev), handle){
		struct fwnode_handle *remote_parent;
		ret = fwnode_property_read_u32(handle, "sink", &sink);
		if(ret || !sink){
			continue;
		}

		ret = fwnode_property_read_u32(handle, "reg", &reg);
		if(ret)
			continue;
		if(reg != id)
			continue;

		remote_handle = fwnode_graph_get_remote_endpoint(handle);
		if(!remote_handle){
			fwnode_handle_put(handle);
			return -EINVAL;
		}

		/* Handle cases were parent is not available (status != ok in dt) */
		remote_parent = fwnode_graph_get_port_parent(remote_handle);
		if (remote_parent && !fwnode_device_is_available(remote_parent)) {
			pr_info("skipping remote-ep %s because its device is not available\n",
				 remote_handle->ops->get_name(remote_handle));
			continue;
		}

		if((med_dev = hailo15_media_get_endpoint(remote_handle)) == NULL){
			fwnode_handle_put(handle);
			fwnode_handle_put(remote_handle);
			return -ENOENT;
		}
		fwnode_handle_put(handle);
		fwnode_handle_put(remote_handle);
		*sd = med_dev->sd;
		return 0;
	}

	return -ENOENT;
}
EXPORT_SYMBOL(hailo15_media_get_subdev);

int hailo15_media_create_connections(struct device* dev, struct v4l2_subdev* sd){
	struct hailo15_media_connection* connection;
	struct fwnode_handle *handle, *remote_handle;
	struct fwnode_endpoint fwnode_ep, fwnode_remote_ep;
	struct hailo15_media_device *sink_mdev, *source_mdev;
	int ret = 0;
	int sink = 0;

	fwnode_graph_for_each_endpoint(dev_fwnode(dev), handle){
		struct fwnode_handle *remote_parent;
		ret = fwnode_property_read_u32(handle, "sink", &sink);
		if(ret || !sink){
			if(!ret){
				source_mdev = kzalloc(sizeof(struct hailo15_media_device), GFP_KERNEL);
				if(!source_mdev){
					fwnode_handle_put(handle);
					return -ENOMEM;
				}
				source_mdev->sd = sd;
				source_mdev->endpoint = handle;
				ret = fwnode_graph_parse_endpoint(handle, &fwnode_ep);
				if (ret) {
					pr_err("failed to parse source-ep %s of subdev %s, skipping...",
						handle->ops->get_name(handle), sd->name);
					continue;
				}

				pr_info("Registering source-ep: [%s, port %d, id %d]\n", 
					sd->name, fwnode_ep.port, fwnode_ep.id);
				hailo15_media_register_subdevice(source_mdev);
			}
			continue;
		}

		remote_handle = fwnode_graph_get_remote_endpoint(handle);
		if(!remote_handle){
			pr_err("failed to get remote-ep connected to sink-ep: [%s, port %d, id %d], aborting with -EINVAL",
				sd->name, fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(handle);
			return -EINVAL;
		}

		ret = fwnode_graph_parse_endpoint(remote_handle, &fwnode_remote_ep);
		if (ret) {
			pr_notice("failed to parse remote-ep connected to sink-ep: [%s, port %d, id %d], skipping sink-ep ...",
				sd->name, fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(remote_handle);
			continue;			
		}

		/* Handle cases were parent is not available (status != ok in dt) */
		remote_parent = fwnode_graph_get_port_parent(remote_handle);
		if (!remote_parent) {
			pr_notice("failed to get parent of remote-ep connected to sink-ep: [%s, port %d, id %d], skipping sink-ep ...",
				 sd->name, fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(remote_handle);
			continue;			
		}

		if (!fwnode_device_is_available(remote_parent)) {
			pr_info("remote-ep: [%s, port %d, id %d] connected to sink-ep: [%s, port %d, id %d], parent remote-ep not available, skipping...\n",
				 remote_parent->ops->get_name(remote_parent), fwnode_remote_ep.port, fwnode_remote_ep.id, 
				 sd->name, fwnode_ep.port, fwnode_ep.id);
			fwnode_handle_put(remote_parent);
			fwnode_handle_put(remote_handle);
			continue;
		}

		if((source_mdev = hailo15_media_get_endpoint(remote_handle)) == NULL){
			pr_err("remote-ep: [%s, port %d, id %d] connected to sink-ep: [%s, port %d, id %d], is not registered, aborting...\n",
				 remote_parent->ops->get_name(remote_parent), fwnode_remote_ep.port, fwnode_remote_ep.id, 
				 sd->name, fwnode_ep.port, fwnode_ep.id);
			ret = -ENOENT;
			goto out_loop;
		}
		sink_mdev = kzalloc(sizeof(struct hailo15_media_device), GFP_KERNEL);
		if(!sink_mdev){
			ret = -ENOMEM;
			goto out_loop;
		}

		sink_mdev->sd = sd;
		sink_mdev->endpoint = handle;

		connection = kzalloc(sizeof(struct hailo15_media_connection), GFP_KERNEL);
		if(!connection){
			kfree(sink_mdev);
			ret = -ENOMEM;
			goto out_loop;
		}

		connection->source = source_mdev;
		connection->sink = sink_mdev;
		mutex_lock(&mdev_lock);
		list_add_tail(&connection->connection, &connections);
		mutex_unlock(&mdev_lock);

out_loop:
		fwnode_handle_put(remote_handle);
		fwnode_handle_put(remote_parent);
		if(ret){
			fwnode_handle_put(handle);
			return ret;
		}

	}

	return 0;
}
EXPORT_SYMBOL(hailo15_media_create_connections);

int hailo15_media_create_links(struct device* dev, struct media_entity* entity, int id){

	struct fwnode_handle *handle, *remote_handle;
	struct hailo15_media_device *med_dev;
	struct v4l2_fwnode_link link;
	int ret = 0;
	int sink = 0;
	int reg = 0;

	memset(&link, 0, sizeof(link));
	fwnode_graph_for_each_endpoint(dev_fwnode(dev), handle){
		struct fwnode_handle *remote_parent;
		ret = fwnode_property_read_u32(handle, "sink", &sink);
		if(ret || !sink){
			continue;
		}

		if(id != -1){
			ret = fwnode_property_read_u32(handle, "reg", &reg);
			if(ret)
				continue;
			if(reg != id)
				continue;
		}

		remote_handle = fwnode_graph_get_remote_endpoint(handle);
		if(!remote_handle){
			fwnode_handle_put(handle);
			return -EINVAL;
		}

		/* Handle cases were parent is not available (status != ok in dt) */
		remote_parent = fwnode_graph_get_port_parent(remote_handle);
		if (remote_parent && !fwnode_device_is_available(remote_parent)) {
			pr_info("skipping remote-ep %s because its device is not available\n",
				 remote_handle->ops->get_name(remote_handle));
			continue;
		}

		if((med_dev = hailo15_media_get_endpoint(remote_handle)) == NULL){
			ret = -ENOENT;
			goto out_loop;
		}
		if (!med_dev->sd->entity.graph_obj.mdev) {
			ret = -ENODEV;
			dev_err(dev, "Remote source subdevice %s failed to register\n", med_dev->sd->name);
			goto out_loop;
		}
		ret = v4l2_fwnode_parse_link(remote_handle, &link);
		if(ret){
			goto out_loop;
		}

		ret = media_create_pad_link(&med_dev->sd->entity, link.local_port, entity, id == -1 ? link.remote_port : link.remote_port - id, MEDIA_LNK_FL_ENABLED);
		if(ret){
			v4l2_fwnode_put_link(&link);
			goto out_loop;
		}

		v4l2_fwnode_put_link(&link);
out_loop:
		fwnode_handle_put(remote_handle);
		if(ret){
			fwnode_handle_put(handle);
			return ret;
		}
	}

	return 0;

}
EXPORT_SYMBOL(hailo15_media_create_links);

int hailo15_media_register_video_subdev_nodes(struct v4l2_device *video_dev) {
	return v4l2_device_register_subdev_nodes(video_dev);
}
EXPORT_SYMBOL(hailo15_media_register_video_subdev_nodes);

void hailo15_media_entity_clean(struct media_entity* entity){
	struct list_head *pos, *npos;
	struct hailo15_media_connection* connection;
	struct hailo15_media_device* mdev;
	mutex_lock(&mdev_lock);
	list_for_each_safe(pos, npos, &connections){
		connection = list_entry(pos, struct hailo15_media_connection, connection);
		if(&connection->sink->sd->entity == entity || &connection->source->sd->entity == entity){
			list_del(&connection->connection);
			if(&connection->sink->sd->entity == entity)
				kfree(connection->sink);

			kfree(connection);
		}
	}

	list_for_each_safe(pos, npos, &endpoints_list){
		mdev = list_entry(pos, struct hailo15_media_device, link);
		if(&mdev->sd->entity == entity){
			list_del(&mdev->link);
			kfree(mdev);
		}
	}
	mutex_unlock(&mdev_lock);
	media_entity_cleanup(entity);
}
EXPORT_SYMBOL(hailo15_media_entity_clean);


void hailo15_media_clean_media_device(void){
	media_device_unregister(&mdev);
	media_device_cleanup(&mdev);
	init = 0;
}
EXPORT_SYMBOL(hailo15_media_clean_media_device);

static int hailo15_media_init(void){
	INIT_LIST_HEAD(&endpoints_list);
	INIT_LIST_HEAD(&connections);
	memset(&mdev, 0, sizeof(mdev));
	strscpy(mdev.model, "hailo_media",
		sizeof(mdev.model));
	mutex_init(&mdev_lock);
	init = 0;
	return 0;
}
module_init(hailo15_media_init);

static void hailo15_media_exit(void){
	hailo15_media_clean_media_device();
	mutex_destroy(&mdev_lock);
}
module_exit(hailo15_media_exit);

MODULE_DESCRIPTION("hailo15 media framework");
MODULE_LICENSE("GPL v2");
