import os, struct, subprocess, sys
from decimal import Decimal

from migen.fhdl.std import *
from migen.fhdl.specials import SynthesisDirective
from migen.genlib.cdc import *
from migen.fhdl.structure import _Fragment

from mibuild.generic_platform import *
from mibuild.crg import SimpleCRG
from mibuild import tools

class CRG_SE(SimpleCRG):
	def __init__(self, platform, clk_name, rst_name, period=None, rst_invert=False):
		SimpleCRG.__init__(self, platform, clk_name, rst_name, rst_invert)
		platform.add_period_constraint(platform, self._clk, period)

class CRG_DS(Module):
	def __init__(self, platform, clk_name, rst_name, period=None, rst_invert=False):
		reset_less = rst_name is None
		self.clock_domains.cd_sys = ClockDomain(reset_less=reset_less)
		self._clk = platform.request(clk_name)
		platform.add_period_constraint(platform, self._clk.p, period)
		self.specials += Instance("IBUFGDS",
			Instance.Input("I", self._clk.p),
			Instance.Input("IB", self._clk.n),
			Instance.Output("O", self.cd_sys.clk)
		)
		if not reset_less:
			if rst_invert:
				self.comb += self.cd_sys.rst.eq(~platform.request(rst_name))
			else:
				self.comb += self.cd_sys.rst.eq(platform.request(rst_name))

def _format_constraint(c):
	if isinstance(c, Pins):
		return "LOC=" + c.identifiers[0]
	elif isinstance(c, IOStandard):
		return "IOSTANDARD=" + c.name
	elif isinstance(c, Drive):
		return "DRIVE=" + str(c.strength)
	elif isinstance(c, Misc):
		return c.misc

def _format_ucf(signame, pin, others, resname):
	fmt_c = []
	for c in [Pins(pin)] + others:
		fc = _format_constraint(c)
		if fc is not None:
			fmt_c.append(fc)
	fmt_r = resname[0] + ":" + str(resname[1])
	if resname[2] is not None:
		fmt_r += "." + resname[2]
	return "NET \"" + signame + "\" " + " | ".join(fmt_c) + "; # " + fmt_r + "\n"

def _build_ucf(named_sc, named_pc):
	r = ""
	for sig, pins, others, resname in named_sc:
		if len(pins) > 1:
			for i, p in enumerate(pins):
				r += _format_ucf(sig + "(" + str(i) + ")", p, others, resname)
		else:
			r += _format_ucf(sig, pins[0], others, resname)
	if named_pc:
		r += "\n" + "\n\n".join(named_pc)
	return r

def _build_xst_files(device, sources, vincpaths, build_name, xst_opt):
	prj_contents = ""
	for filename, language in sources:
		prj_contents += language + " work " + filename + "\n"
	tools.write_to_file(build_name + ".prj", prj_contents)

	xst_contents = """run
-ifn {build_name}.prj
-top top
{xst_opt}
-ofn {build_name}.ngc
-p {device}
""".format(build_name=build_name, xst_opt=xst_opt, device=device)
	for path in vincpaths:
		xst_contents += "-vlgincdir " + path + "\n"
	tools.write_to_file(build_name + ".xst", xst_contents)

def _run_yosys(device, sources, vincpaths, build_name):
	ys_contents = ""
	incflags = ""
	for path in vincpaths:
		incflags += " -I" + path
	for filename, language in sources:
		ys_contents += "read_{}{} {}\n".format(language, incflags, filename)
	
	if device[:2] == "xc":
		archcode = device[2:4]
	else:
		archcode = device[0:2]
	arch = {
		"6s": "spartan6",
		"7a": "artix7",
		"7k": "kintex7",
		"7v": "virtex7",
		"7z": "zynq7000"
	}[archcode]
	
	ys_contents += """hierarchy -check -top top
proc; memory; opt; fsm; opt
synth_xilinx -arch {arch} -top top -edif {build_name}.edif""".format(arch=arch, build_name=build_name)
	
	ys_name = build_name + ".ys"
	tools.write_to_file(ys_name, ys_contents)
	r = subprocess.call(["yosys", ys_name])
	if r != 0:
		raise OSError("Subprocess failed")

def _is_valid_version(path, v):
	try: 
		Decimal(v)
		return os.path.isdir(os.path.join(path, v))
	except:
		return False

def _run_ise(build_name, ise_path, source, mode, ngdbuild_opt,
		bitgen_opt, ise_commands, map_opt, par_opt):
	if sys.platform == "win32" or sys.platform == "cygwin":
		source = False
	build_script_contents = "# Autogenerated by mibuild\nset -e\n"
	if source:
		vers = [ver for ver in os.listdir(ise_path) if _is_valid_version(ise_path, ver)]
		tools_version = max(vers)
		bits = struct.calcsize("P")*8
		
		xilinx_settings_file = os.path.join(ise_path, tools_version, "ISE_DS", "settings{0}.sh".format(bits))
		if not os.path.exists(xilinx_settings_file) and bits == 64:
			# if we are on 64-bit system but the toolchain isn't, try the 32-bit env.
			xilinx_settings_file = os.path.join(ise_path, tools_version, "ISE_DS", "settings32.sh")
		build_script_contents += "source " + xilinx_settings_file + "\n"
	if mode == "edif":
		ext = "edif"
	else:
		ext = "ngc"
		build_script_contents += """
xst -ifn {build_name}.xst"""

	build_script_contents += """
ngdbuild {ngdbuild_opt} -uc {build_name}.ucf {build_name}.{ext} {build_name}.ngd
map {map_opt} -o {build_name}_map.ncd {build_name}.ngd {build_name}.pcf
par {par_opt} {build_name}_map.ncd {build_name}.ncd {build_name}.pcf
bitgen {bitgen_opt} {build_name}.ncd {build_name}.bit
"""
	build_script_contents = build_script_contents.format(build_name=build_name,
			ngdbuild_opt=ngdbuild_opt, bitgen_opt=bitgen_opt, ext=ext,
			par_opt=par_opt, map_opt=map_opt)
	build_script_contents += ise_commands.format(build_name=build_name)
	build_script_file = "build_" + build_name + ".sh"
	tools.write_to_file(build_script_file, build_script_contents, force_unix=True)

	r = subprocess.call(["bash", build_script_file])
	if r != 0:
		raise OSError("Subprocess failed")

class XilinxNoRetimingImpl(Module):
	def __init__(self, reg):
		self.specials += SynthesisDirective("attribute register_balancing of {r} is no", r=reg)

class XilinxNoRetiming:
	@staticmethod
	def lower(dr):
		return XilinxNoRetimingImpl(dr.reg)

class XilinxMultiRegImpl(MultiRegImpl):
	def __init__(self, *args, **kwargs):
		MultiRegImpl.__init__(self, *args, **kwargs)
		self.specials += [SynthesisDirective("attribute shreg_extract of {r} is no", r=r)
			for r in self.regs]

class XilinxMultiReg:
	@staticmethod
	def lower(dr):
		return XilinxMultiRegImpl(dr.i, dr.o, dr.odomain, dr.n)

class XilinxISEPlatform(GenericPlatform):
	bitstream_ext = ".bit"
	xst_opt = """-ifmt MIXED
-opt_mode SPEED
-register_balancing yes"""
	map_opt = "-ol high -w"
	par_opt = "-ol high -w"
	ngdbuild_opt = ""
	bitgen_opt = "-g LCK_cycle:6 -g Binary:Yes -w"
	ise_commands = ""
	def get_verilog(self, *args, special_overrides=dict(), **kwargs):
		so = {
			NoRetiming: XilinxNoRetiming,
			MultiReg:   XilinxMultiReg
		}
		so.update(special_overrides)
		return GenericPlatform.get_verilog(self, *args, special_overrides=so, **kwargs)

	def get_edif(self, fragment, **kwargs):
		return GenericPlatform.get_edif(self, fragment, "UNISIMS", "Xilinx", self.device, **kwargs)

	def build(self, fragment, build_dir="build", build_name="top",
			ise_path="/opt/Xilinx", source=True, run=True, mode="xst"):
		tools.mkdir_noerror(build_dir)
		os.chdir(build_dir)

		if not isinstance(fragment, _Fragment):
			fragment = fragment.get_fragment()
		self.finalize(fragment)

		ngdbuild_opt = self.ngdbuild_opt

		if mode == "xst" or mode == "yosys":
			v_src, named_sc, named_pc = self.get_verilog(fragment)
			v_file = build_name + ".v"
			tools.write_to_file(v_file, v_src)
			sources = self.sources + [(v_file, "verilog")]
			if mode == "xst":
				_build_xst_files(self.device, sources, self.verilog_include_paths, build_name, self.xst_opt)
				isemode = "xst"
			else:
				_run_yosys(self.device, sources, self.verilog_include_paths, build_name)
				isemode = "edif"
				ngdbuild_opt += "-p " + self.device

		if mode == "mist":
			from mist import synthesize
			synthesize(fragment, self.constraint_manager.get_io_signals())

		if mode == "edif" or mode == "mist":
			e_src, named_sc, named_pc = self.get_edif(fragment)
			e_file = build_name + ".edif"
			tools.write_to_file(e_file, e_src)
			isemode = "edif"

		tools.write_to_file(build_name + ".ucf", _build_ucf(named_sc, named_pc))
		if run:
			_run_ise(build_name, ise_path, source, isemode,
					ngdbuild_opt, self.bitgen_opt, self.ise_commands,
					self.map_opt, self.par_opt)

		os.chdir("..")

	def add_period_constraint(self, clk, period):
		if period is not None:
			self.add_platform_command("""NET "{clk}" TNM_NET = "GRP{clk}";
TIMESPEC "TS{clk}" = PERIOD "GRP{clk}" """+str(period)+""" ns HIGH 50%;""", clk=clk)
