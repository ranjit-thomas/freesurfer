import os
import numpy as np
import tensorflow as tf
import keras.backend as K
from keras.optimizers import Adam
from keras.models import Model, Sequential
from keras.layers import *
from keras.utils.training_utils import multi_gpu_model
from _utility import dice_coef_loss2
K.set_image_data_format('channels_last')


def unet_model_3d(input_shape, num_filters, unet_depth, downsize_filters_factor=1, pool_size=(2, 2, 2), n_labels=0,
                  loss='mean_absolute_error', initial_learning_rate=0.00001, deconvolution=False, use_patches=True, num_gpus=1):
    """
    Builds the 3D UNet Keras model.
    :param input_shape: Shape of the input data (x_size, y_size, z_size).
    :param downsize_filters_factor: Factor to which to reduce the number of filters. Making this value larger will
    reduce the amount of memory the model will need during training.
    :param pool_size: Pool size for the max pooling operations.
    :param n_labels: Number of binary labels that the model is learning.
    :param initial_learning_rate: Initial learning rate for the model. This will be decayed during training.
    :param deconvolution: If set to True, will use transpose convolution(deconvolution) instead of upsamping. This
    increases the amount memory required during training.
    :return: Untrained 3D UNet Model
    """
    # channels last, make feature shape from (32,32,32) - > (32,32,32,1)

    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    # input_shape_list = list(input_shape)
    # input_shape_list.append(1)
    # input_shape_append = tuple(input_shape_list)
    print(input_shape)
    input_img = Input(shape=input_shape, name='input' )
    convs = []
    pools = []
    inputs = []
    centered_inputs = []
    endpoints = []
    print('unet depth is ')
    print(unet_depth)
    for i in range(unet_depth):

        prev = input_img if i == 0 else pools[i-1]
        print(int(num_filters*(2**i)/downsize_filters_factor))
        conv = Conv3D(int(num_filters*(2**i)/downsize_filters_factor), (3, 3, 3),
                      activation='relu', padding='same', kernel_initializer="he_normal",
                      name=('conv3D_D_1_%d' % (i)))(prev)
        conv = BatchNormalization(name=('bnorm_D_1_%d' % (i)))(conv)
        conv = Conv3D(int(num_filters*(2**i)/downsize_filters_factor), (3, 3, 3),
                      activation='relu', padding='same', kernel_initializer="he_normal",
                      name=('conv3D_D_2_%d' % (i)))(conv)
        conv = BatchNormalization(name=('bnorm_D_2_%d' % (i)))(conv)
        if i < (unet_depth - 1):
            pools.append(MaxPooling3D(pool_size, name=('pool_D_%d' % (i)), data_format='channels_last')(conv))

        convs.append(conv)

    for i in range(unet_depth - 1):
        index = i + unet_depth - 1
        level = unet_depth - (i + 2)
        up = concatenate([UpSampling3D(size=pool_size,  name=('upsampling_U_%d' % (level+1)))(convs[index]),
                          convs[level]], axis=-1,  name=('concat_%d' % (level)))
        conv = Conv3D(num_filters * (2 ** level), (3, 3, 3), padding="same", activation="relu",
                      kernel_initializer="he_normal",
                      name=('conv3D_U_1_%d' % (level))
                      )(up)
        conv = BatchNormalization(name=('bnorm_U_1_%d' % (level)))(conv)
        conv = Conv3D(num_filters * (2 ** level), (3, 3, 3), padding="same", activation="relu",
                      kernel_initializer="he_normal",
                      name=('conv3D_U_2_%d' % (level)))(conv)
        convs.append(BatchNormalization(name=('bnorm_U_2_%d' % (level)))(conv))

#    conv = ZeroPadding3D(padding=(1, 1, 1))(convs[-1])
#    conv = Conv3D(num_filters * 2, (3, 3, 3), padding="valid", activation="relu",
#                  kernel_initializer="he_normal")(conv)
#    conv = BatchNormalization()(conv)
#    center_input = Cropping3D(cropping=(0, 0, 0))(input_img)

    inputs.append(input_img)
#    centered_inputs.append(center_input)
    print(convs)
    endpoints.append(convs[-1])


    up = concatenate(inputs + endpoints, axis=-1, name='final_concat')
    print(loss)
    print('is_seg_network' + str(is_seg_network))
    if is_seg_network == False:
        print(loss)
        conv = Conv3D(1, (1,1,1), activation='relu',  name='final_conv_3d')(up)


        if num_gpus > 1:
            with tf.device('/cpu:0'):
                model = Model(inputs=inputs, outputs=conv)
                parallel_model = multi_gpu_model(model, gpus=num_gpus)
                if loss == 'grad_loss':
                    print(loss)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                else:
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)

                return model, parallel_model
        else:
            model = Model(inputs=inputs, outputs=conv)
            if loss == 'grad_loss':
                print(loss)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
            else:
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
            return model, model

    else:
        print('segmentation network')
        if n_labels > 1:
            conv = Conv3D(n_labels, (1, 1, 1), activation='softmax', name='final_conv_3d')(up)

            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=inputs, outputs=conv)

                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate),
                                           loss=dice_coef_loss2, )

                    model.compile(optimizer=Adam(lr=initial_learning_rate),
                                  loss=dice_coef_loss2, )

                    return model, parallel_model
            else:
                model = Model(inputs=inputs, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate),
                              loss=dice_coef_loss2, )
                return model, model
        else:
            conv = Conv3D(1, (1, 1, 1), activation='sigmoid', name='final_conv_3d')(up)

            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=inputs, outputs=conv)

                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate),
                                           loss=dice_coef_loss2, )

                    model.compile(optimizer=Adam(lr=initial_learning_rate),
                                  loss=dice_coef_loss2, )

                    return model, parallel_model
            else:
                model = Model(inputs=inputs, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate),
                              loss=dice_coef_loss2, )
                return model, model


def unet_model_2d(input_shape, num_filters, unet_depth, downsize_filters_factor=1, pool_size=(2, 2), n_labels=0,
                  loss='mean_squared_error', initial_learning_rate=0.00001, deconvolution=False, use_patches=True,
                  num_gpus=1, num_outputs=1):

    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    dim = len(input_shape)
    if dim == 2:
        ConvL =  Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2,2)
        UpSamplingL = UpSampling2D
        filter_shape = (5,5)
        out_filter_shape = (1,1)
    elif dim==3:
        ConvL = Conv3D
        MaxPoolingL= MaxPooling3D
        pool_size = (2,2,2)
        UpSamplingL = UpSampling3D
        filter_shape = (3,3,3)
        out_filter_shape = (1,1,1)

    print('out filter shape is ' + str(out_filter_shape))
    input_shape_list = list(input_shape)
    input_shape_list.append(1)
    input_shape_append = tuple(input_shape_list)
    print(input_shape_append)
    input_img = Input(shape=input_shape_append, name='input' )
    convs = []
    pools = []
    inputs = []
    centered_inputs = []
    endpoints = []

    print('unet depth is ' + unet_depth)
    for i in range(unet_depth):
        prev = input_img if i == 0 else pools[i-1]
        print(int(num_filters*(2**i)/downsize_filters_factor))
        conv = ConvL(int(num_filters*(2**i)/downsize_filters_factor), filter_shape,
                     activation='relu', padding='same', kernel_initializer="he_normal",
                     name=('conv3D_D_1_%d' % (i)))(prev)
        conv = BatchNormalization(name=('bnorm_D_1_%d' % (i)))(conv)
        conv = ConvL(int(num_filters*(2**i)/downsize_filters_factor), filter_shape,
                     activation='relu', padding='same', kernel_initializer="he_normal",
                     name=('conv3D_D_2_%d' % (i)))(conv)
        conv = BatchNormalization(name=('bnorm_D_2_%d' % (i)))(conv)
        if i < (unet_depth - 1):
            pools.append(MaxPoolingL(pool_size, name=('pool_D_%d' % (i)), data_format='channels_last')(conv))
        convs.append(conv)

    for i in range(unet_depth - 1):
        index = i + unet_depth - 1
        level = unet_depth - (i + 2)
        up = concatenate([UpSamplingL(size=pool_size,  name=('upsampling_U_%d' % (level+1)))(convs[index]),
                          convs[level]], axis=-1,  name=('concat_%d' % (level)))
        conv = ConvL(num_filters * (2 ** level), filter_shape, padding="same", activation="relu",
                     kernel_initializer="he_normal", name=('conv3D_U_1_%d' % (level)))(up)
        conv = BatchNormalization(name=('bnorm_U_1_%d' % (level)))(conv)
        conv = ConvL(num_filters * (2 ** level), filter_shape, padding="same", activation="relu",
                     kernel_initializer="he_normal", name=('conv3D_U_2_%d' % (level)))(conv)
        convs.append(BatchNormalization(name=('bnorm_U_2_%d' % (level)))(conv))

    # conv = ZeroPadding3D(padding=(1, 1, 1))(convs[-1])
    # conv = Conv3D(num_filters * 2, (3, 3, 3), padding="valid", activation="relu", kernel_initializer="he_normal")(conv)
    # conv = BatchNormalization()(conv)
    # center_input = Cropping3D(cropping=(0, 0, 0))(input_img)

    inputs.append(input_img)
    # centered_inputs.append(center_input)
    endpoints.append(convs[-1])
    up = concatenate(inputs + endpoints, axis=-1, name='final_concat')

    print(convs)
    print(loss)
    print('is_seg_network' + str(is_seg_network))

    if is_seg_network == False:
        print(loss)
        conv = ConvL(num_outputs, out_filter_shape, activation='linear',  name='final_conv_3d')(up)

        if num_gpus > 1:
            with tf.device('/cpu:0'):
                model = Model(inputs=inputs, outputs=conv)
                parallel_model = multi_gpu_model(model, gpus=num_gpus)
                if loss == 'grad_loss':
                    print(loss)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                else:
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)

                return model, parallel_model
        else:
            model = Model(inputs=inputs, outputs=conv)
            if loss == 'grad_loss':
                print(loss)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
            else:
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
            return model, model
    else:
        print('segmentation network')
        if n_labels > 1:
            conv = ConvL(n_labels, out_filter_shape, activation='softmax', name='final_conv_3d')(up)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=inputs, outputs=conv)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    return model, parallel_model
            else:
                model = Model(inputs=inputs, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                return model, model
        else:
            conv = ConvL(1, (1, 1, 1), activation='sigmoid', name='final_conv_3d')(up)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=inputs, outputs=conv)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    return model, parallel_model
            else:
                model = Model(inputs=inputs, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                return model, model




def unet_2d_v1(input_shape, num_filters, unet_depth, downsize_filters_factor=1, pool_size=(2, 2), n_labels=0,
               loss='mean_squared_error', initial_learning_rate=0.00001, deconvolution=False, num_gpus=1, num_outputs=1):

    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    dim = len(input_shape)
    if dim == 3:
        ConvL =  Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2,2)
        UpSamplingL = UpSampling2D
        filter_shape = (5,5)
        out_filter_shape = (1,1)
    elif dim==4:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        pool_size = (2,2,2)
        UpSamplingL = UpSampling3D
        filter_shape = (3,3,3)
        out_filter_shape = (1,1,1)

    input_img_ax = Input(shape=input_shape, name='input_ax' )
    conv_ax = build_oriented_unet_2d(input_img_ax, input_shape=input_shape, num_filters=num_filters,
                                     unet_depth=unet_depth, layer_prefix='', downsize_filters_factor=downsize_filters_factor,
                                     pool_size=pool_size, n_labels=n_labels, num_outputs=num_outputs)
    (model_ax, parallel_model_ax) = build_compile_model(input_img_ax, input_shape, conv_ax, n_labels,
                                                        loss, num_gpus, initial_learning_rate)
    return model_ax, parallel_model_ax


def build_oriented_unet_2d(input_layer, input_shape, num_filters, unet_depth, layer_prefix='',
                           downsize_filters_factor=1,
                           pool_size=(2, 2), n_labels=0, num_outputs=1, num_gpus=1):
    """
    Args:
        input_img_ax: input layer
        input_shape: (256,256,1)
        num_filters: initial number of filters
        unet_depth: number of poolings
        downsize_filters_factor: generallly 1
        pool_size: (2,2)
        n_labels: number of labels to predict. 0 if regression
        num_outputs: dimensionality of outouts. 1 if single regression. 2 if vector valued output

    Returns:
        callable final layer

    """
    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    dim = len(input_shape)
    if dim == 3:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2, 2)
        UpSamplingL = UpSampling2D
        filter_shape = (5, 5)
        out_filter_shape = (1, 1)
    elif dim == 4:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        pool_size = (2, 2, 2)
        UpSamplingL = UpSampling3D
        filter_shape = (3, 3, 3)
        out_filter_shape = (1, 1, 1)

    print('out filter shape is ' + str(out_filter_shape))

    convs = []
    pools = []
    inputs = []
    endpoints = []

    print('unet depth is ')
    print(unet_depth)

    for i in range(unet_depth):
        prev = input_layer if i == 0 else pools[i - 1]
        print(int(num_filters * (2 ** i) / downsize_filters_factor))
        conv = ConvL(int(num_filters * (2 ** i) / downsize_filters_factor), filter_shape,
                     activation='relu', padding='same', kernel_initializer="he_normal",
                     name=('conv3D_D_1_%d' % (i) + layer_prefix))(prev)
        conv = BatchNormalization(name=('bnorm_D_1_%d' % (i) + layer_prefix))(conv)
        conv = ConvL(int(num_filters * (2 ** i) / downsize_filters_factor), filter_shape,
                     activation='relu', padding='same', kernel_initializer="he_normal",
                     name=('conv3D_D_2_%d' % (i) + layer_prefix))(conv)
        conv = BatchNormalization(name=('bnorm_D_2_%d' % (i) + layer_prefix))(conv)
        if i < (unet_depth - 1):
            pools.append(
                MaxPoolingL(pool_size, name=('pool_D_%d' % (i) + layer_prefix), data_format='channels_last')(conv))
        convs.append(conv)

    for i in range(unet_depth - 1):
        index = i + unet_depth - 1
        level = unet_depth - (i + 2)
        up = concatenate(
            [UpSamplingL(size=pool_size, name=('upsampling_U_%d' % (level + 1) + layer_prefix))(convs[index]),
             convs[level]], axis=-1, name=('concat_%d' % (level) + layer_prefix))
        conv = ConvL(num_filters * (2 ** level), filter_shape, padding="same", activation="relu",
                     kernel_initializer="he_normal",
                     name=('conv3D_U_1_%d' % (level) + layer_prefix))(up)
        conv = BatchNormalization(name=('bnorm_U_1_%d' % (level) + layer_prefix))(conv)
        conv = ConvL(num_filters * (2 ** level), filter_shape, padding="same", activation="relu",
                     kernel_initializer="he_normal",
                     name=('conv3D_U_2_%d' % (level) + layer_prefix))(conv)
        convs.append(BatchNormalization(name=('bnorm_U_2_%d' % (level) + layer_prefix))(conv))

        # conv = ZeroPadding3D(padding=(1, 1, 1))(convs[-1])
        # conv = Conv3D(num_filters * 2, (3, 3, 3), padding="valid", activation="relu", kernel_initializer="he_normal")(conv)
        # conv = BatchNormalization()(conv)
        # center_input = Cropping3D(cropping=(0, 0, 0))(input_img)

    inputs.append(input_layer)
    # centered_inputs.append(center_input)
    print(convs)
    endpoints.append(convs[-1])
    up = concatenate(inputs + endpoints, axis=-1, name='final_concat' + layer_prefix)

    print('is_seg_network' + str(is_seg_network))
    if is_seg_network == False:
        conv = ConvL(num_outputs, out_filter_shape, activation='linear', name='final_conv_3d' + layer_prefix)(up)
    else:
        print('segmentation network')
        if n_labels > 1:
            conv = ConvL(n_labels, out_filter_shape, activation='softmax', name='final_conv_3d' + layer_prefix)(up)
        else:
            conv = ConvL(1, out_filter_shape, activation='sigmoid', name='final_conv_3d' + layer_prefix)(up)

    return conv


def build_compile_model(input_layer, input_shape, conv, n_labels, loss, num_gpus, initial_learning_rate):
    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    dim = len(input_shape)
    if dim == 3:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2, 2)
        UpSamplingL = UpSampling2D
        filter_shape = (5, 5)
        out_filter_shape = (1, 1)

    elif dim == 4:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        pool_size = (2, 2, 2)
        UpSamplingL = UpSampling3D
        filter_shape = (3, 3, 3)
        out_filter_shape = (1, 1, 1)

    if is_seg_network == False:
        print(loss)
        # conv = ConvL(1, out_filter_shape, activation='relu',  name='final_conv_3d')(conv_penultimate)
        if num_gpus > 1:
            with tf.device('/cpu:0'):
                model = Model(inputs=input_layer, outputs=conv)
                parallel_model = multi_gpu_model(model, gpus=num_gpus)
                if loss == 'grad_loss':
                    print(loss)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                else:
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                return model, parallel_model
        else:
            model = Model(inputs=input_layer, outputs=conv)
            if loss == 'grad_loss':
                print(loss)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
            else:
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
            return model, model

    else:
        print('segmentation network')
        if n_labels > 1:
            # conv = ConvL(n_labels, out_filter_shape, activation='softmax', name='final_conv_3d')(conv_penultimate)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=input_layer, outputs=conv)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2, )
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2, )
                    return model, parallel_model
            else:
                model = Model(inputs=input_layer, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2, )
                return model, model
        else:
            # conv = ConvL(1,out_filter_shape, activation='sigmoid', name='final_conv_3d')(conv_penultimate)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=input_layer, outputs=conv)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2, )
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2, )
                    return model, parallel_model
            else:
                model = Model(inputs=input_layer, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2, )
                return model, model




def unet_encoder_dense(feature_shape, output_shape, unet_num_filters, depth, depth_per_level,
                       n_labels, initial_learning_rate, loss='binary_crossentropy'):

    if len(feature_shape) == 3:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        filter_shape = (3,3)
        pool_shape = (2,2)
        conv1x1_shape = (1,1)
    elif len(feature_shape) == 4:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        filter_shape = (3,3,3)
        pool_shape = (2,2,2)
        conv1x1_shape = (1, 1, 1)

    if n_labels != output_shape[-1]:
        print('Number of labels do not match the output shape last dimension')
        return None

    min_feature_dim = np.min(feature_shape[0:-1])
    if 2**depth > min_feature_dim:
        print('Reduce depth of the network to fit '+ str(depth) + ' poolings')
        return None

    num_output_dense_filters = np.prod(output_shape)

    input_shape_list = list(feature_shape)
    # input_shape_list.append(1)
    input_shape_append = tuple(input_shape_list)
    print(input_shape_append)
    model = Sequential()
    for iter_layer in range(depth):
        if iter_layer == 0:
            model.add(ConvL(unet_num_filters*(2**iter_layer), filter_shape, padding='same', activation='relu',
                input_shape=input_shape_append, kernel_initializer="he_normal"))
            for iter_depth_per_layer in range(depth_per_level-1):
                model.add(ConvL(unet_num_filters*(2**iter_layer), filter_shape, padding='same',
                                activation='relu', kernel_initializer="he_normal"))
            model.add(MaxPoolingL(pool_size=pool_shape))
            model.add(Dropout(0.25))
        else:
            for iter_depth_per_layer in range(depth_per_level):
                model.add(ConvL(unet_num_filters*(2**iter_layer), filter_shape, padding='same',
                                activation='relu', kernel_initializer="he_normal"))
            model.add(MaxPoolingL(pool_size=pool_shape))
            model.add(Dropout(0.25))

    model.add(Flatten())
    model.add(Dense(512, activation='relu', kernel_initializer="he_normal"))
    model.add(Dropout(0.5))
    model.add(Dense(256, activation='relu', kernel_initializer="he_normal"))
    model.add(Dropout(0.5))

    if n_labels == 1:
        model.add(Dense(num_output_dense_filters, activation='relu', kernel_initializer="he_normal"))
        model.add(Reshape(output_shape))
    elif n_labels > 1:
        model.add(Dense(num_output_dense_filters, activation='relu', kernel_initializer="he_normal"))
        model.add(Reshape(output_shape))
        model.add(ConvL(n_labels, conv1x1_shape, activation='softmax', kernel_initializer="he_normal"))

    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
    return model


def unet_model_2d_noBN(input_shape, num_filters, unet_depth, downsize_filters_factor=1, pool_size=(2, 2), n_labels=0,
                  loss='mean_squared_error', initial_learning_rate=0.00001, deconvolution=False, use_patches=True,
                  num_gpus=1, num_outputs=1):

    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    dim = len(input_shape)
    if dim == 3:
        ConvL =  Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2,2)
        UpSamplingL = UpSampling2D
        filter_shape = (5,5)
        out_filter_shape = (1,1)

    elif dim==4:
        ConvL = Conv3D
        MaxPoolingL= MaxPooling3D
        pool_size = (2,2,2)
        UpSamplingL = UpSampling3D
        filter_shape = (3,3,3)
        out_filter_shape = (1,1,1)

    print('out filter shape is ' + str(out_filter_shape))
    # input_shape_list = list(input_shape)
    # input_shape_list.append(1)
    # input_shape_append = tuple(input_shape_list)
    # print(input_shape_append)
    input_img = Input(shape=input_shape, name='input' )
    convs = []
    pools = []
    inputs = []
    centered_inputs = []
    endpoints = []

    print('unet depth is ')
    print(unet_depth)
    for i in range(unet_depth):

        prev = input_img if i == 0 else pools[i-1]
        print(int(num_filters*(2**i)/downsize_filters_factor))
        conv = ConvL(int(num_filters*(2**i)/downsize_filters_factor), filter_shape,
                      activation='relu', padding='same', kernel_initializer="he_normal",
                      name=('conv3D_D_1_%d' % (i)))(prev)
        # conv = BatchNormalization(name=('bnorm_D_1_%d' % (i)))(conv)
        conv = ConvL(int(num_filters*(2**i)/downsize_filters_factor), filter_shape,
                      activation='relu', padding='same', kernel_initializer="he_normal",
                      name=('conv3D_D_2_%d' % (i)))(conv)
        # conv = BatchNormalization(name=('bnorm_D_2_%d' % (i)))(conv)
        if i < (unet_depth - 1):
            pools.append(MaxPoolingL(pool_size, name=('pool_D_%d' % (i)), data_format='channels_last')(conv))

        convs.append(conv)

    for i in range(unet_depth - 1):
        index = i + unet_depth - 1
        level = unet_depth - (i + 2)
        up = concatenate([UpSamplingL(size=pool_size,  name=('upsampling_U_%d' % (level+1)))(convs[index]),
                          convs[level]], axis=-1,  name=('concat_%d' % (level)))
        conv = ConvL(num_filters * (2 ** level), filter_shape, padding="same", activation="relu",
                      kernel_initializer="he_normal",
                      name=('conv3D_U_1_%d' % (level))
                      )(up)
        # conv = BatchNormalization(name=('bnorm_U_1_%d' % (level)))(conv)
        conv = ConvL(num_filters * (2 ** level), filter_shape, padding="same", activation="relu",
                      kernel_initializer="he_normal",
                      name=('conv3D_U_2_%d' % (level)))(conv)
        # convs.append(BatchNormalization(name=('bnorm_U_2_%d' % (level)))(conv))
        convs.append(conv)

    # conv = ZeroPadding3D(padding=(1, 1, 1))(convs[-1])
    # conv = Conv3D(num_filters * 2, (3, 3, 3), padding="valid", activation="relu", kernel_initializer="he_normal")(conv)
    # conv = BatchNormalization()(conv)
    # center_input = Cropping3D(cropping=(0, 0, 0))(input_img)

    inputs.append(input_img)
    # centered_inputs.append(center_input)
    print(convs)
    endpoints.append(convs[-1])

    up = concatenate(inputs + endpoints, axis=-1, name='final_concat')
    print(loss)
    print('is_seg_network' + str(is_seg_network))
    if is_seg_network == False:
        print(loss)
        conv = ConvL(num_outputs, out_filter_shape, activation='linear',  name='final_conv_3d')(up)
        if num_gpus > 1:
            with tf.device('/cpu:0'):
                model = Model(inputs=inputs, outputs=conv)
                parallel_model = multi_gpu_model(model, gpus=num_gpus)
                if loss == 'grad_loss':
                    print(loss)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                else:
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                return model, parallel_model
        else:
            model = Model(inputs=inputs, outputs=conv)
            if loss == 'grad_loss':
                print(loss)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
            elif loss == 'warp_image_loss':
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=warp_image_loss)
            else:
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
            return model, model
    else:
        print('segmentation network')
        if n_labels > 1:
            conv = ConvL(n_labels, out_filter_shape, activation='softmax', name='final_conv_3d')(up)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=inputs, outputs=conv)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    return model, parallel_model
            else:
                model = Model(inputs=inputs, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                return model, model
        else:
            conv = Conv3D(1, (1, 1, 1), activation='sigmoid', name='final_conv_3d')(up)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=inputs, outputs=conv)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    return model, parallel_model
            else:
                model = Model(inputs=inputs, outputs=conv)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                return model, model


def class_net_v1(feature_shape, dim, unet_num_filters, depth, depth_per_level, n_labels, initial_learning_rate, loss='binary_crossentropy'):

    if dim == 2:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        filter_shape = (3,3)
        pool_shape = (2,2)
    elif dim ==3:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        filter_shape = (3,3,3)
        pool_shape = (2,2,2)

    min_feature_dim = np.min(feature_shape[0:-1])
    if 2**depth > min_feature_dim:
        print('Reduce depth of the network to fit '+ str(depth) + ' poolings')
        return None

    input_shape_list = list(feature_shape)
    # input_shape_list.append(1)
    input_shape_append = tuple(input_shape_list)
    print(input_shape_append)
    model = Sequential()
    for iter_layer in range(depth):
        if iter_layer == 0:
            model.add(ConvL(unet_num_filters*(2**iter_layer), filter_shape, padding='same', activation='relu',
                            input_shape=input_shape_append, kernel_initializer="he_normal"))
            for iter_depth_per_layer in range(depth_per_level-1):
                model.add(ConvL(unet_num_filters*(2**iter_layer), filter_shape, padding='same', activation='relu', kernel_initializer="he_normal"))
            model.add(MaxPoolingL(pool_size=pool_shape))
            model.add(Dropout(0.25))
        else:
            for iter_depth_per_layer in range(depth_per_level):
                model.add(ConvL(unet_num_filters*(2**iter_layer), filter_shape, padding='same', activation='relu', kernel_initializer="he_normal"))
            model.add(MaxPoolingL(pool_size=pool_shape))
            model.add(Dropout(0.25))

    model.add(Flatten())
    model.add(Dense(512, activation='relu', kernel_initializer="he_normal"))
    model.add(Dropout(0.5))
    model.add(Dense(n_labels, activation='softmax'))

    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss, metrics=['accuracy'])
    return model


def class_net(feature_shape, dim, unet_num_filters, n_labels, initial_learning_rate, loss='binary_crossentropy'):

    if dim == 2:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        filter_shape = (3,3)
        pool_shape = (2,2)
    elif dim ==3:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        filter_shape = (3,3,3)
        pool_shape = (2,2,2)

    input_shape_list = list(feature_shape)
    # input_shape_list.append(1)
    input_shape_append = tuple(input_shape_list)
    print(input_shape_append)

    model = Sequential()
    model.add(ConvL(unet_num_filters, filter_shape, padding='same', activation='relu', input_shape=input_shape_append))
    model.add(ConvL(unet_num_filters, filter_shape, padding='valid', activation='relu'))
    model.add(MaxPoolingL(pool_size=pool_shape))
    model.add(Dropout(0.25))

    model.add(ConvL(2*unet_num_filters, filter_shape, padding='same', activation='relu'))
    model.add(ConvL(2*unet_num_filters, filter_shape, activation='relu'))
    model.add(MaxPoolingL(pool_size=pool_shape))
    model.add(Dropout(0.25))

    model.add(ConvL((2**2)*unet_num_filters, filter_shape, padding='same', activation='relu'))
    model.add(ConvL((2**2)*unet_num_filters, filter_shape, activation='relu'))
    model.add(MaxPoolingL(pool_size=pool_shape))
    model.add(Dropout(0.25))

    model.add(ConvL((2**3)*unet_num_filters, filter_shape, padding='same', activation='relu'))
    model.add(ConvL((2**3)*unet_num_filters, filter_shape, activation='relu'))
    model.add(MaxPoolingL(pool_size=pool_shape))
    model.add(Dropout(0.25))

    model.add(ConvL((2**4)*unet_num_filters, filter_shape, padding='same', activation='relu'))
    model.add(ConvL((2**4)*unet_num_filters, filter_shape, activation='relu'))
    model.add(MaxPoolingL(pool_size=pool_shape))
    model.add(Dropout(0.25))

    model.add(Flatten())
    model.add(Dense(512, activation='relu'))
    model.add(Dropout(0.5))

    if n_labels == 2:
        model.add(Dense(1, activation='sigmoid'))
    else:
        model.add(Dense(n_labels, activation='softmax'))

    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss, metrics=['accuracy'])
    return model


def atrous_net(input_shape, num_filters,initial_learning_rate=0.00001, loss='mean_absolute_error'):

    input_shape_list = list(input_shape)
    input_shape_list.append(1)
    input_shape_append = tuple(input_shape_list)
    print(input_shape_append)
    input_img = Input(shape=input_shape_append, name='input' )
    convs = []
    pools = []
    inputs = []
    centered_inputs = []
    endpoints = []

    x = Conv3D(num_filters, (3, 3, 3), activation='relu', padding='same', dilation_rate=1)(input_img)
    x = Conv3D(num_filters, (3, 3, 3), activation='relu', padding='same', dilation_rate=1)(x)
    x = Conv3D(num_filters, (3, 3, 3), activation='relu', padding='same', dilation_rate=2)(x)
    x = Conv3D(num_filters, (3, 3, 3), activation='relu', padding='same', dilation_rate=4)(x)
    x = Conv3D(1, (1, 1, 1), activation='relu', name='final_conv_3d')(x)

    model = Model(inputs=input_img, outputs=x)
    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
    return model


def resnet_model(input_shape, num_filters, unet_depth, downsize_filters_factor=1, pool_size=(2, 2, 2), n_labels=0,
                  loss='mean_absolute_error', initial_learning_rate=0.00001, deconvolution=False, use_patches=True, num_gpus=1):

    if n_labels > 0:
        is_seg_network = True
    else:
        is_seg_network = False

    dim = len(input_shape)
    if dim == 3:
        ConvL =  Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2,2)
        UpSamplingL = UpSampling2D
        filter_shape = (5,5)
        out_filter_shape = (1,1)

    elif dim==4:
        ConvL = Conv3D
        MaxPoolingL= MaxPooling3D
        pool_size = (2,2,2)
        UpSamplingL = UpSampling3D
        filter_shape = (3,3,3)
        out_filter_shape = (1,1,1)

    print(input_shape)
    input_img = Input(shape=input_shape, name='input' )

    x = ConvL(16, (3, 3, 3), padding='same', name='conv1')(input_img)
    x = BatchNormalization(name='bn_conv1')(x)
    x = Activation('relu')(x)
    x = ConvL(16, (3, 3, 3), padding='same', name='conv2')(input_img)
    x = BatchNormalization(name='bn_conv2')(x)
    x = Activation('relu')(x)

    # x = niftynet_block(x, [16, 16], stage=2, block='b', dilation_rate=1)
    # x = niftynet_block(x, [16, 16], stage=2, block='c', dilation_rate=1)

    # x = niftynet_block(x, [32, 32], stage=3, block='b', dilation_rate=2)
    # x = niftynet_block(x, [32, 32], stage=3, block='c', dilation_rate=2)

    # x = niftynet_block(x, [64, 64], stage=4, block='b', dilation_rate=4)
    # x = niftynet_block(x, [64, 64], stage=4, block='c', dilation_rate=4)

    # x = MaxPooling3D((3, 3, 3), strides=(2, 2, 2))(x)

    x = conv_block(x, [16, 16, 16], stage=2, block='a', strides=(1, 1, 1), dilation_rate=1)
    x = conv_block(x, [16, 16, 16], stage=2, block='b', strides=(1, 1, 1), dilation_rate=1)
    x = conv_block(x, [16, 16, 16], stage=2, block='c', strides=(1, 1, 1), dilation_rate=1)


    # x = identity_block(x, [16, 16, 16], stage=2, block='b', dilation_rate=1)
    # x = identity_block(x, [16, 16, 16], stage=2, block='c', dilation_rate=1)

    x = conv_block(x,  [32, 32, 32], stage=3, block='a', strides=(1, 1, 1), dilation_rate=2)
    x = conv_block(x, [32, 32, 32], stage=3, block='b', strides=(1, 1, 1), dilation_rate=2)
    x = conv_block(x, [32, 32, 32], stage=3, block='c', strides=(1, 1, 1), dilation_rate=2)

    x = conv_block(x,  [64, 64, 64], stage=4, block='a', strides=(1, 1, 1), dilation_rate=4)
    x = conv_block(x, [64, 64, 64], stage=4,  block='b', strides=(1, 1, 1), dilation_rate=4)
    x = conv_block(x, [64, 64, 64], stage=4, block='c', strides=(1, 1, 1), dilation_rate=4)

    # x = identity_block(x, [32, 32, 32], stage=3, block='b', dilation_rate=2)
    # x = identity_block(x, [32, 32, 32], stage=3, block='c', dilation_rate=2)
    # x = identity_block(x, [16, 32, 32], stage=3, block='d', dilation_rate=2)

    # x = conv_block(x, [16, 32, 32], stage=4, block='a', strides=(1, 1, 1), dilation_rate=4)
    # x = identity_block(x,  [16, 32, 32], stage=4, block='b', dilation_rate=4)
    # x = identity_block(x,  [16, 32, 32], stage=4, block='c', dilation_rate=4)
    # x = identity_block(x,  [16, 32, 32], stage=4, block='d', dilation_rate=4)
    # x = identity_block(x,  [16, 32, 32], stage=4, block='e',  dilation_rate=4)
    # x = identity_block(x,  [16, 32, 32], stage=4, block='f',  dilation_rate=4)

    # x = conv_block(x,  [16, 32, 32], stage=5, block='a', strides=(1, 1, 1), dilation_rate=8)
    # x = identity_block(x, [16, 32, 32], stage=5, block='b',  dilation_rate=8)
    # x = identity_block(x, [512, 512, 2048], stage=5, block='c',  dilation_rate=2)

    # decoding block

    print(loss)
    print('is_seg_network' + str(is_seg_network))
    if is_seg_network == False:
        print(loss)
        x = ConvL(1, (1, 1, 1), activation='relu', name='final_conv_3d')(x)

        if num_gpus > 1:
            with tf.device('/cpu:0'):
                model = Model(inputs=input_img, outputs=x)
                parallel_model = multi_gpu_model(model, gpus=num_gpus)
                if loss == 'grad_loss':
                    print(loss)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
                else:
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
                return model, parallel_model
        else:
            model = Model(inputs=input_img, outputs=x)
            if loss == 'grad_loss':
                print(loss)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=grad_loss)
            else:
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=loss)
            return model, model

    else:
        print('segmentation network')
        if n_labels > 1:
            x = ConvL(n_labels, (1, 1, 1), activation='softmax', name='final_conv_3d')(x)

            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=input_img, outputs=x)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    return model, parallel_model
            else:
                model = Model(inputs=input_img, outputs=x)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                return model, model
        else:
            x = ConvL(1, (1, 1, 1), activation='sigmoid', name='final_conv_3d')(x)
            if num_gpus > 1:
                with tf.device('/cpu:0'):
                    model = Model(inputs=input_img, outputs=x)
                    parallel_model = multi_gpu_model(model, gpus=num_gpus)
                    parallel_model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                    return model, parallel_model
            else:
                model = Model(inputs=input_img, outputs=x)
                model.compile(optimizer=Adam(lr=initial_learning_rate), loss=dice_coef_loss2,)
                return model, model


def identity_block(input_tensor, filters, stage, block, dilation_rate):
    """The identity block is the block that has no conv layer at shortcut.

    Args:
        input_tensor: input tensor
        kernel_size: default 3, the kernel size of middle conv layer at main path
        filters: list of integers, the filters of 3 conv layer at main path
        stage: integer, current stage label, used for generating layer names
        block: 'a','b'..., current block label, used for generating layer names
    
    Returns:
        Output tensor for the block.

    """
    dim = len(input_tensor.shape)
    if dim == 4:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2, 2)
        UpSamplingL = UpSampling2D
        filter_shape = (5, 5)
        onexone_filter_shape = (1, 1)
        out_filter_shape = (1, 1)
        strides = (2,2)
    elif dim == 5:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        pool_size = (2, 2, 2)
        UpSamplingL = UpSampling3D
        filter_shape = (3, 3, 3)
        onexone_filter_shape = (1, 1, 1)
        out_filter_shape = (1, 1, 1)
        strides = (2,2,2)

    filters1, filters2, filters3 = filters
    if K.image_data_format() == 'channels_last':
        bn_axis = -1
    else:
        bn_axis = 1
    conv_name_base = 'res' + str(stage) + block + '_branch'
    bn_name_base = 'bn' + str(stage) + block + '_branch'

    x = ConvL(filters1, onexone_filter_shape, padding='same', dilation_rate=dilation_rate, name=conv_name_base + '2a')(input_tensor)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2a')(x)
    x = Activation('relu')(x)

    x = ConvL(filters2, filter_shape, padding='same', dilation_rate=dilation_rate, name=conv_name_base + '2b')(x)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2b')(x)
    x = Activation('relu')(x)

    x = ConvL(filters3, onexone_filter_shape, dilation_rate=dilation_rate, name=conv_name_base + '2c')(x)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2c')(x)

    x = Add()([x, input_tensor ])
    x = Activation('relu')(x)
    return x


def conv_block(input_tensor, filters, stage, block, strides, dilation_rate):
    """A block that has a conv layer at shortcut.

    Note:
        From stage 3, the first conv layer at main path is with strides=(2, 2).
        The shortcut should have strides=(2, 2) as well
    
    Arguments:
        input_tensor: Input tensor
        filters: List of integers, the filters of 3 conv layer at main path
        stage: Integer, current stage label, used for generating layer names
        block: 'a','b'..., current block label, used for generating layer names
        strides: Strides for the first conv layer in the block

    Returns:
        Output tensor for the block.

    """
    dim = len(input_tensor.shape)
    if dim == 4:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2, 2)
        UpSamplingL = UpSampling2D
        filter_shape = (5, 5)
        onexone_filter_shape = (1, 1)
        out_filter_shape = (1, 1)
        strides = strides
    elif dim == 5:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        pool_size = (2, 2, 2)
        UpSamplingL = UpSampling3D
        filter_shape = (3, 3, 3)
        onexone_filter_shape = (1, 1, 1)
        out_filter_shape = (1, 1, 1)
        strides = strides

    filters1, filters2, filters3 = filters
    if K.image_data_format() == 'channels_last':
        bn_axis = -1
    else:
        bn_axis = 1
    conv_name_base = 'res' + str(stage) + block + '_branch'
    bn_name_base = 'bn' + str(stage) + block + '_branch'

    x = ConvL(filters1, onexone_filter_shape, padding='same', dilation_rate=dilation_rate, name=conv_name_base + '2a')(input_tensor)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2a')(x)
    x = Activation('relu')(x)

    x = ConvL(filters2, filter_shape, padding='same',dilation_rate=dilation_rate, name=conv_name_base + '2b')(x)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2b')(x)
    x = Activation('relu')(x)

    x = ConvL(filters3, onexone_filter_shape, dilation_rate=dilation_rate, name=conv_name_base + '2c')(x)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2c')(x)

    shortcut = ConvL(filters3, onexone_filter_shape, dilation_rate=dilation_rate, name=conv_name_base + '1')(input_tensor)
    shortcut = BatchNormalization(axis=bn_axis, name=bn_name_base + '1')(shortcut)

    x = Add()([shortcut, x])
    x = Activation('relu')(x)
    return x


def niftynet_block(input_tensor, filters, stage, block, dilation_rate):

    dim = len(input_tensor.shape)
    if dim == 4:
        ConvL = Conv2D
        MaxPoolingL = MaxPooling2D
        pool_size = (2, 2)
        UpSamplingL = UpSampling2D
        filter_shape = (5, 5)
        onexone_filter_shape = (1, 1)
        out_filter_shape = (1, 1)
        strides = (2, 2)
    elif dim == 5:
        ConvL = Conv3D
        MaxPoolingL = MaxPooling3D
        pool_size = (2, 2, 2)
        UpSamplingL = UpSampling3D
        filter_shape = (3, 3, 3)
        onexone_filter_shape = (1, 1, 1)
        out_filter_shape = (1, 1, 1)
        strides = (2, 2, 2)

    filters1, filters2 =  filters
    if K.image_data_format() == 'channels_last':
        bn_axis = -1
    else:
        bn_axis = 1
    conv_name_base = 'res' + str(stage) + block + '_branch'
    bn_name_base = 'bn' + str(stage) + block + '_branch'

    x = ConvL(filters1, filter_shape, padding='same', dilation_rate=dilation_rate, name=conv_name_base + '2a')(input_tensor)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2a')(x)
    x = Activation('relu')(x)

    x = ConvL(filters2, filter_shape, padding='same', dilation_rate=dilation_rate, name=conv_name_base + '2b')(x)
    x = BatchNormalization(axis=bn_axis, name=bn_name_base + '2b')(x)
    x = Activation('relu')(x)

    x = Add()([x, input_tensor])
    x = Activation('relu')(x)
    return x


def compute_level_output_shape(filters, depth, pool_size, image_shape):
    """
    Each level has a particular output shape based on the number of filters used in that level and the depth or number
    of max pooling operations that have been done on the data at that point.

    Args:
        image_shape: shape of the 3d image.
        pool_size: the pool_size parameter used in the max pooling operation.
        filters: Number of filters used by the last node in a given level.
        depth: The number of levels down in the U-shaped model a given node is.
        
    Returns:
        A 5D vector of the shape of the output node

    """
    if depth != 0:
        output_image_shape = np.divide(image_shape, np.multiply(pool_size, depth)).tolist()
    else:
        output_image_shape = image_shape
    return tuple([None, filters] + [int(x) for x in output_image_shape])


def get_upconv(depth, nb_filters, pool_size, image_shape, kernel_size=(2, 2, 2), strides=(2, 2, 2), deconvolution=False):
    if deconvolution:
        input_shape  = compute_level_output_shape(filters=nb_filters, depth=depth+1, pool_size=pool_size, image_shape=image_shape)
        output_shape = compute_level_output_shape(filters=nb_filters, depth=depth, pool_size=pool_size, image_shape=image_shape)
        return Deconvolution3D(filters=nb_filters, kernel_size=kernel_size, output_shape=output_shape, strides=strides, input_shape=input_shape)
    else:
        return UpSampling3D(size=pool_size)
