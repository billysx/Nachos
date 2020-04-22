"""
@author: Viet Nguyen <nhviet1009@gmail.com>
"""

import os
os.environ['OMP_NUM_THREADS'] = '1'
import argparse
import torch
from optimizer import *
from process import local_train, local_test
import torch.multiprocessing as _mp
import shutil
import a3c
import env
import load_trace
import numpy as np



def train(args):
    print(args)
    torch.manual_seed(123)
    if os.path.isdir(args.log_path):
        shutil.rmtree(args.log_path)
    os.makedirs(args.log_path)
    if not os.path.isdir(args.saved_path):
        os.makedirs(args.saved_path)
    mp = _mp.get_context("spawn")

    global_model = a3c.A3C(state_dim=[args.s_info, args.s_len],
                            action_dim=args.a_dim,
                            learning_rate=[args.actor_lr, args.critic_lr])

    global_model._initialize_weights()


if __name__ == "__main__":
    args = get_args()
    train(args)