#!/usr/bin/env python

"""
Testing script for TFTP server.

To use this script install tftp-hpa (or other compatible) client and modify CLIENT_DIR,
SERVER_DIR, TFTP_PORT and TFTP_IP constants.

Random files are created in CLIENT_DIR and uploaded to the server, then downloaded.
Also uploading many files in parallel is tested.

Functions invoke, rnd_mode, put_file and get_file communicates with server, every class
with create method represents single file. Functions two_clients, multiple_clients and
putting_and_getting are functions to test the server.

putting_and_getting receives list of file classes, creates those files in CLIENT_DIR,
uploads them to server, check if uploading succeeded, then removes them from CLIENT_DIR
and downloads from the server and finally checks if download succeeded.

two_clients and multiple_clients are functions that run multiple clients in parallel.

Note: If you want to use other client than tftp-hpa, modify TFTP_CLIENT_BIN and
you may also want to modify invoke function.
"""

import random
import os
import filecmp
import subprocess
import threading
import sys

CLIENT_DIR = "/home/mayfa/tftp_client/"
SERVER_DIR = "/home/mayfa/tftp_server/"
# tftp-hpa client.
TFTP_CLIENT_BIN = "tftp"
TFTP_IP = "127.0.0.1"
TFTP_PORT = "4567"

MODE_NETASCII = "netascii"
MODE_OCTET = "octet"


def file_cmp(filename, operation):
    """ Compares file in client dir to (uploaded) file in server dir. """
    if filecmp.cmp(CLIENT_DIR + filename, SERVER_DIR + filename):
        print(operation + " succeeded, filename = " + filename)
    else:
        print(operation + " failed, filename = " + filename, file=sys.stderr)
    return


def try_remove(filepath):
    """ Removes the file if it exists. """
    if os.path.isfile(filepath):
        os.remove(filepath)
    return


def invoke(cmd, filename, mode=MODE_NETASCII, log=True, verbose=False, trace=False):
    """
    Invokes put (upload) or get (download) command on tftp client - creates a
    command file unique for every thread and passes it as stdin to tftp client.
    Cases when tftp client fails execution are logged.
    :param cmd: put or get
    :param filename:
    :param mode: MODE_NETASCII or MODE_OCTET
    :param verbose: specify verbose mode for tftp client.
    :param trace: specify trace mode for tftp client.
    """
    cmd_file_name = "cmd_file." + threading.current_thread().getName()

    # Create command file.
    cmd_file = open(cmd_file_name, "w")
    if verbose:
        cmd_file.write("verbose\n")
    if trace:
        cmd_file.write("trace\n")
    cmd_file.write("mode " + mode + "\n")
    cmd_file.write(cmd + " " + filename + "\n")
    cmd_file.write("quit")
    cmd_file.close()

    # Log.
    if log:
        print(cmd + " " + filename + "(" + mode + ")")

    # Run tftp client.
    cmd_file = open(cmd_file_name, "r")
    process = subprocess.run([TFTP_CLIENT_BIN, TFTP_IP, TFTP_PORT], stdin=cmd_file,
                             stdout=subprocess.DEVNULL)
    if process.returncode != 0:
        print(TFTP_CLIENT_BIN + " failed", file=sys.stderr)
    cmd_file.close()

    # Remove command file.
    os.remove(os.getcwd() + "/" + cmd_file_name)
    return


def rnd_mode():
    rnd_num = random.random()
    if rnd_num <= 0.5:
        return MODE_NETASCII
    else:
        return MODE_OCTET


def put_file(filename, mode=MODE_NETASCII, log=True):
    invoke("put", filename, mode, log)


def get_file(filename, mode=MODE_NETASCII, log=True):
    invoke("get", filename, mode, log)


class File1:
    name = "file1.txt"

    def create(self):
        file = open(self.name, "w")
        for i in range(400):
            if i % 2 == 0:
                file.write("\n")
            else:
                file.write("\r")
        file.close()
        return


class AFile:
    name = "afile.txt"

    def create(self):
        file = open(self.name, "w")
        for i in range(1):
            file.write("a")
        file.close()
        return


class NonAsciiFile:
    name = "nonascii.bin"

    def create(self):
        file = open(self.name, "w")
        for i in range(1500):
            file.write(chr(random.randrange(0,255)))
        file.close()


class BigRndFile:
    name = "big_rnd_file"

    def create(self):
        file = open(self.name, "w")
        for i in range(2 * 1024 * 1000):
            file.write(str(random.randrange(0, 127)))
        file.close()
        return


class BigFile:
    name = "big_file.txt"

    def create(self):
        file = open(self.name, "w")
        str = ""
        for i in range(50 * 1024 * 1000):
            str += "a"
        file.write(str)
        file.close()
        return


class CtrlFile:
    name = "ctrl_file.bin"

    def create(self):
        """ Random file containing only control characters. """
        file = open(self.name, "w")
        for i in range(10000):
            rnd = random.randint(1, 3)
            if rnd == 1:
                file.write("\0")
            elif rnd == 2:
                file.write("\r")
            elif rnd == 3:
                file.write("\n")
        file.close()
        return


def two_clients():
    """
    Run two uploads at once.
    :param big_file_name name of big_file
    :param small_file_name name of small file
    """
    # Construct big and small files.
    bigfile_inst = BigFile()
    bigfile_inst.create()

    smallfile_inst = File1()
    smallfile_inst.create()

    # Remove both files from server dir from previous tests.
    try_remove(SERVER_DIR + bigfile_inst.name)
    try_remove(SERVER_DIR + smallfile_inst.name)

    # Specify callable objects, otherwise thread is called immediately
    # during construction.
    class PutBigFile:
        def __call__(self):
            put_file(bigfile_inst.name)

    class PutSmallFile:
        def __call__(self):
            put_file(smallfile_inst.name)

    put_big_file_inst = PutBigFile()
    put_small_file_inst = PutSmallFile()

    # Put both files at once.
    thread1 = threading.Thread(name="Lazy client", target=put_big_file_inst)
    thread2 = threading.Thread(name="Quick client", target=put_small_file_inst)
    thread1.start()
    thread2.start()
    thread1.join()
    thread2.join()

    # Check upload.
    file_cmp(bigfile_inst.name, "Upload")
    file_cmp(smallfile_inst.name, "Upload")

    # Remove files.
    os.remove(SERVER_DIR + bigfile_inst.name)
    os.remove(SERVER_DIR + smallfile_inst.name)
    os.remove(CLIENT_DIR + bigfile_inst.name)
    os.remove(CLIENT_DIR + smallfile_inst.name)
    return


def multiple_clients(n):
    """
    AFile must be created.
    """
    a_fname = "a.txt"
    appended_fname = "appended.txt"

    try_remove(SERVER_DIR + a_fname)

    # Create a file.
    file = open(a_fname, "w")
    file.write("a")
    file.close()

    class PutAFile:
        def __call__(self):
            put_file(a_fname, log=False)

    # Initialize callable instances and threads arrays.
    insts = []
    threads = []
    for i in range(n):
        insts.append(PutAFile())
        threads.append(threading.Thread(target=insts[i]))

    # Start all threads.
    for thread in threads:
        thread.start()

    # Join all threads.
    for thread in threads:
        thread.join()

    # Create appended file.
    appended_file = open(appended_fname, "w")
    for i in range(n):
        appended_file.write("a")
    appended_file.close()

    # Check upload.
    if filecmp.cmp(appended_fname, SERVER_DIR + a_fname):
        print("Parallel clients test succeeded")
    else:
        print("Parallel clients test failed", file=sys.stderr)

    # Remove files.
    os.remove(SERVER_DIR + a_fname)
    os.remove(CLIENT_DIR + a_fname)
    os.remove(CLIENT_DIR + appended_fname)
    return


def putting_and_getting(file_classes):
    """ Tests putting and getting files to (from) the server. """
    for FileClass in file_classes:
        # Create instance and delete file from server dir.
        instance = FileClass()
        instance.create()
        try_remove(SERVER_DIR + instance.name)
        # Put (upload) and compare.
        put_file(instance.name, rnd_mode())
        file_cmp(instance.name, "Upload")
        # Remove file from client's dir.
        os.remove(CLIENT_DIR + instance.name)
        # Get (download) and compare.
        get_file(instance.name, rnd_mode())
        file_cmp(instance.name, "Download")
        # Remove file from server dir and client dir.
        os.remove(SERVER_DIR + instance.name)
        os.remove(CLIENT_DIR + instance.name)
    return


file_class_list = [File1, AFile, BigRndFile, CtrlFile, NonAsciiFile]


os.chdir(CLIENT_DIR)
putting_and_getting(file_class_list)
two_clients()
multiple_clients(150)

