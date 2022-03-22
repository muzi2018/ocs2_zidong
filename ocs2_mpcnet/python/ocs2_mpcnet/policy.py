###############################################################################
# Copyright (c) 2022, Farbod Farshidian. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
#  * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
###############################################################################

"""Policy classes.

Provides classes for different neural network policies.
"""

import torch

from ocs2_mpcnet.helper import bmv


class LinearPolicy(torch.nn.Module):
    """Linear policy.

    Class for a simple linear neural network policy.

    Attributes:
        name: A string with the name of the policy.
        dim_in: An integer defining the input dimension of the policy.
        dim_out: An integer defining the output dimension of the policy.
        linear: The linear neural network layer.
    """
    def __init__(self, dim_t, dim_x, dim_u):
        """Initializes the LinearPolicy class.

        Initializes the LinearPolicy class by setting fixed and variable attributes.

        Args:
            dim_t: An integer defining the generalized time dimension.
            dim_x: An integer defining the relative state dimension.
            dim_u: An integer defining the control input dimension.
        """
        super().__init__()
        self.name = "LinearPolicy"
        self.dim_in = dim_t + dim_x
        self.dim_out = dim_u
        self.linear = torch.nn.Linear(self.dim_in, self.dim_out)

    def forward(self, t, x):
        """Forward method.

        Defines the computation performed at every call. Computes the output tensors from the input tensors.

        Args:
            t: A (B,T) tensor with the generalized times.
            x: A (B,X) tensor with the relative states.

        Returns:
            u: A (B,U) tensor with the predicted control inputs.
        """
        return self.linear(torch.cat((t, x), dim=1))


class NonlinearPolicy(torch.nn.Module):
    """Nonlinear policy.

    Class for a simple nonlinear neural network policy, where the hidden layer is the mean of the input and output layer.

    Attributes:
        name: A string with the name of the policy.
        dim_in: An integer defining the input dimension of the policy.
        dim_hidden: An integer defining the dimension of the hidden layer.
        dim_out: An integer defining the output dimension of the policy.
        linear1: The first linear neural network layer.
        activation: The activation to get the hidden layer.
        linear2: The second linear neural network layer.
    """
    def __init__(self, dim_t, dim_x, dim_u):
        """Initializes the NonlinearPolicy class.

        Initializes the NonlinearPolicy class by setting fixed and variable attributes.

        Args:
            dim_t: An integer defining the generalized time dimension.
            dim_x: An integer defining the relative state dimension.
            dim_u: An integer defining the control input dimension.
        """
        super().__init__()
        self.name = "NonlinearPolicy"
        self.dim_in = dim_t + dim_x
        self.dim_hidden = int((dim_t + dim_x + dim_u) / 2)
        self.dim_out = dim_u
        self.linear1 = torch.nn.Linear(self.dim_in, self.dim_hidden)
        self.activation = torch.nn.Tanh()
        self.linear2 = torch.nn.Linear(self.dim_hidden, self.dim_out)

    def forward(self, t, x):
        """Forward method.

        Defines the computation performed at every call. Computes the output tensors from the input tensors.

        Args:
            t: A (B,T) tensor with the generalized times.
            x: A (B,X) tensor with the relative states.

        Returns:
            u: A (B,U) tensor with the predicted control inputs.
        """
        return self.linear2(self.activation(self.linear1(torch.cat((t, x), dim=1))))


class MixtureOfLinearExpertsPolicy(torch.nn.Module):
    """Mixture of linear experts policy.

    Class for a mixture of experts neural network with linear experts.

    Attributes:
        name: A string with the name of the policy.
        dim_in: An integer defining the input dimension of the policy.
        dim_out: An integer defining the output dimension of the policy.
        num_experts: An integer defining the number of experts.
        gating_net: The gating network.
        expert_nets: The expert networks.
    """
    def __init__(self, dim_t, dim_x, dim_u, num_experts):
        """Initializes the MixtureOfLinearExpertsPolicy class.

        Initializes the MixtureOfLinearExpertsPolicy class by setting fixed and variable attributes.

        Args:
            dim_t: An integer defining the generalized time dimension.
            dim_x: An integer defining the relative state dimension.
            dim_u: An integer defining the control input dimension.
            num_experts: An integer defining the number of experts.
        """
        super().__init__()
        self.name = "MixtureOfLinearExpertsPolicy"
        self.dim_in = dim_t + dim_x
        self.dim_out = dim_u
        self.num_experts = num_experts
        # gating
        self.gating_net = torch.nn.Sequential(torch.nn.Linear(self.dim_in, self.num_experts), torch.nn.Softmax(dim=1))
        # experts
        self.expert_nets = torch.nn.ModuleList(
            [LinearExpert(i, self.dim_in, self.dim_out) for i in range(self.num_experts)]
        )

    def forward(self, t, x):
        """Forward method.

        Defines the computation performed at every call. Computes the output tensors from the input tensors.

        Args:
            t: A (B,T) tensor with the generalized times.
            x: A (B,X) tensor with the relative states.

        Returns:
            u: A (B,U) tensor with the predicted control inputs.
            p: A (B,E) tensor with the predicted expert weights.
        """
        p = self.gating_net(torch.cat((t, x), dim=1))
        U = torch.stack([self.expert_nets[i](torch.cat((t, x), dim=1)) for i in range(self.num_experts)], dim=2)
        u = bmv(U, p)
        return u, p


class MixtureOfNonlinearExpertsPolicy(torch.nn.Module):
    """Mixture of nonlinear experts policy.

    Class for a mixture of experts neural network with nonlinear experts, where the hidden layer is the mean of the
    input and output layer.

    Attributes:
        name: A string with the name of the policy.
        dim_in: An integer defining the input dimension of the policy.
        dim_hidden_gating: An integer defining the dimension of the hidden layer for the gating network.
        dim_hidden_expert: An integer defining the dimension of the hidden layer for the expert networks.
        dim_out: An integer defining the output dimension of the policy.
        num_experts: An integer defining the number of experts.
        gating_net: The gating network.
        expert_nets: The expert networks.
    """
    def __init__(self, dim_t, dim_x, dim_u, num_experts):
        """Initializes the MixtureOfNonlinearExpertsPolicy class.

        Initializes the MixtureOfNonlinearExpertsPolicy class by setting fixed and variable attributes.

        Args:
            dim_t: An integer defining the generalized time dimension.
            dim_x: An integer defining the relative state dimension.
            dim_u: An integer defining the control input dimension.
            num_experts: An integer defining the number of experts.
        """
        super().__init__()
        self.name = "MixtureOfNonlinearExpertsPolicy"
        self.dim_in = dim_t + dim_x
        self.dim_hidden_gating = int((dim_t + dim_x + num_experts) / 2)
        self.dim_hidden_expert = int((dim_t + dim_x + dim_u) / 2)
        self.dim_out = dim_u
        self.num_experts = num_experts

        # gating
        self.gating_net = torch.nn.Sequential(
            torch.nn.Linear(self.dim_in, self.dim_hidden_gating),
            torch.nn.Tanh(),
            torch.nn.Linear(self.dim_hidden_gating, self.num_experts),
            torch.nn.Softmax(dim=1),
        )
        # experts
        self.expert_nets = torch.nn.ModuleList(
            [NonlinearExpert(i, self.dim_in, self.dim_hidden_expert, self.dim_out) for i in range(self.num_experts)]
        )

    def forward(self, t, x):
        """Forward method.

        Defines the computation performed at every call. Computes the output tensors from the input tensors.

        Args:
            t: A (B,T) tensor with the generalized times.
            x: A (B,X) tensor with the relative states.

        Returns:
            u: A (B,U) tensor with the predicted control inputs.
            p: A (B,E) tensor with the predicted expert weights.
        """
        p = self.gating_net(torch.cat((t, x), dim=1))
        U = torch.stack([self.expert_nets[i](torch.cat((t, x), dim=1)) for i in range(self.num_experts)], dim=2)
        u = bmv(U, p)
        return u, p


class LinearExpert(torch.nn.Module):
    """Linear expert.

    Class for a simple linear neural network expert.

    Attributes:
        name: A string with the name of the expert.
        dim_in: An integer defining the input dimension of the expert.
        dim_out: An integer defining the output dimension of the expert.
        linear: The linear neural network layer.
    """
    def __init__(self, index, dim_in, dim_out):
        """Initializes the LinearExpert class.

        Initializes the LinearExpert class by setting fixed and variable attributes.

        Args:
            index: An integer with the index of the expert.
            dim_in: An integer defining the input dimension of the expert.
            dim_out: An integer defining the output dimension of the expert.
        """
        super().__init__()
        self.name = "LinearExpert" + str(index)
        self.dim_in = dim_in
        self.dim_out = dim_out
        self.linear = torch.nn.Linear(self.dim_in, self.dim_out)

    def forward(self, input):
        """Forward method.

        Defines the computation performed at every call. Computes the output tensors from the input tensors.

        Args:
            input: A (B,I) tensor with the inputs.

        Returns:
            output: A (B,O) tensor with the outputs.
        """
        return self.linear(input)


class NonlinearExpert(torch.nn.Module):
    """Nonlinear expert.

    Class for a simple nonlinear neural network expert, where the hidden layer is the mean of the input and output layer.

    Attributes:
        name: A string with the name of the expert.
        dim_in: An integer defining the input dimension of the expert.
        dim_hidden: An integer defining the dimension of the hidden layer.
        dim_out: An integer defining the output dimension of the expert.
        linear1: The first linear neural network layer.
        activation: The activation to get the hidden layer.
        linear2: The second linear neural network layer.
    """
    def __init__(self, index, dim_in, dim_hidden, dim_out):
        """Initializes the NonlinearExpert class.

        Initializes the NonlinearExpert class by setting fixed and variable attributes.

        Args:
            index: An integer with the index of the expert.
            dim_in: An integer defining the input dimension of the expert.
            dim_hidden: An integer defining the dimension of the hidden layer.
            dim_out: An integer defining the output dimension of the expert.
        """
        super().__init__()
        self.name = "NonlinearExpert" + str(index)
        self.dim_in = dim_in
        self.dim_hidden = dim_hidden
        self.dim_out = dim_out
        self.linear1 = torch.nn.Linear(self.dim_in, self.dim_hidden)
        self.activation = torch.nn.Tanh()
        self.linear2 = torch.nn.Linear(self.dim_hidden, self.dim_out)

    def forward(self, input):
        """Forward method.

        Defines the computation performed at every call. Computes the output tensors from the input tensors.

        Args:
            input: A (B,I) tensor with the inputs.

        Returns:
            output: A (B,O) tensor with the outputs.
        """
        return self.linear2(self.activation(self.linear1(input)))
